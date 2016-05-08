/*
 *  Copyright (C) loadletter @ https://github.com/loadletter/tox-xd
 *  Copyright (C) Ronan Bignaux
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <ftw.h>
#include <sodium.h>

#include "misc.h"
#include "fileop.h"
#include "ylog/ylog.h"

#define HASHING_BUFSIZE 64 * 1024 /* make it bigger? */
#define SHRDIR_MAX_DESCRIPTORS 6

static FileNode **shr_list = NULL;
static uint32_t shr_list_len = 0;
static FileNode **new_list = NULL;
static uint32_t new_list_len = 0;
static volatile sig_atomic_t file_recheck = false;
/* callback for new file */
static void (*file_new_c)(FileNode *, int) = NULL;
/* prototypes */
static int filenode_dump(FileNode *fnode, char *path);

static int file_checksumcalc_noblock(uint8_t *BLAKE2b, char *filename)
{
    static FILE *f = NULL;
    uint32_t i;
    int rc;
    uint8_t buf[HASHING_BUFSIZE];
    static crypto_generichash_state state;

    if(!f)
    {
        if(!(f = fopen(filename, "rb")))
        {
            perrlog("fopen");
            return(-1);
        }
        //        state = sodium_malloc(crypto_generichash_statebytes());
        //        buf = malloc(HASHING_BUFSIZE);
        crypto_generichash_init(&state, NULL, 0, TOX_FILE_ID_LENGTH);
    }

    if((i = fread(buf, 1, HASHING_BUFSIZE, f)) > 0)
    {
        crypto_generichash_update(&state, buf, HASHING_BUFSIZE);
        rc = i;
    }
    else
    {
        crypto_generichash_final(&state, BLAKE2b, TOX_FILE_ID_LENGTH);
        if(fclose(f) != 0)
            perrlog("fclose");
        f = NULL;
        rc = 0;
    }


    /* rc > 0: still hashing
     * rc == 0: complete
     * rc < 0: error*/

    return(rc);
}

/* Run on every file of the shared dir, if a file is not alredy in the shared array
 *  or has different size/mtime the new_list array is enlarged and a new
 *  fileinfo struct is allocated */
static int file_walk_callback(const char *path, const struct stat *sptr, int type)
{
    uint32_t i;
    int32_t n = -1;
    if (type == FTW_DNR)
        ywarn("Directory %s cannot be traversed.\n", path);
    if (type == FTW_F)
    {
        for(i=0;i<shr_list_len;i++)
        {
            if((strcmp(path, shr_list[i]->file) == 0) &&\
                    (sptr->st_mtime == shr_list[i]->mtime) &&\
                    (sptr->st_size == shr_list[i]->length))
            {
                n = i;
                break;
            }
        }

        if(n == -1)
        {
            new_list = realloc(new_list, sizeof(FileNode *) * (new_list_len + 1));
            if(new_list == NULL)
            {
                perrlog("realloc");
                return -1;
            }
            new_list[new_list_len] = malloc(sizeof(FileNode));
            if(new_list[new_list_len] == NULL)
            {
                perrlog("malloc");
                return -1;
            }
            new_list[new_list_len]->file = strdup(path);
            new_list[new_list_len]->BLAKE2b = NULL;
            new_list[new_list_len]->mtime = sptr->st_mtime;
            new_list[new_list_len]->length = sptr->st_size;
            new_list[new_list_len]->exists = true;
            new_list_len++;
        }
    }
    return 0;
}

int file_walk_shared(const char *shrdir)
{
    char *abspath = NULL;
    int rc;

    abspath = canonicalize_file_name(shrdir);
    if(!abspath)
    {
        perrlog("canonicalize_file_name");
        return -1;
    }
    rc = ftw(abspath, file_walk_callback, SHRDIR_MAX_DESCRIPTORS);
    free(abspath);

    /* no new files found */
    if(new_list_len == 0)
        file_recheck = false;

    return rc;
}

/* walks the current shared files and check if they still exist */
void file_exists_shared(void)
{
    uint32_t i;

    for(i=0;i<shr_list_len;i++)
    {
        if(access(shr_list[i]->file, R_OK) == -1)
        {
            shr_list[i]->exists = false;
            ydebug("Removed non-existing file: %s", shr_list[i]->file);
        }
        else
        {
            shr_list[i]->exists = true;
        }
    }
}

/* signal handler */
void file_recheck_callback(int signo)
{
    if(signo == SIGUSR1)
        file_recheck = true;
}

/* If the recheck flag is set and there is no hashing operations running
 * walks the directory.
 *
 * If there are new files and nothing is currently hashing pick the latest
 * file from the new_list and check if it's alredy in shr_list:
 * - if totally new, reallocate the shared list to accomodate the new
 * - if preexisting in shr_list, free the old and point it to the data of the new
 * then shrink the new_list, removing the last element (frees it when it's empty),
 * finally allocate a FileHash struct for the new element in shr_list and set a flag for hashing
 *
 * If the hashing flag was set, take it's number and start the hashing,
 * when the entire file has been read or an error occurs clear the hashing flag.*/
int file_do(const char *shrdir, const char *cachedir)
{
    uint32_t i, t;
    int32_t rc, n = -1;
    static int hashing = -1;
    static int last;

    if(file_recheck && new_list_len == 0 && hashing == -1)
    {
        file_exists_shared();
        file_walk_shared(shrdir);
    }

    i = new_list_len - 1; /* Starts from last element */
    if(new_list_len > 0 && hashing == -1)
    {
        for(t=0;t<shr_list_len;t++)
        {
            if(strcmp(new_list[i]->file, shr_list[t]->file) == 0)
            {
                n = i;
                break;
            }
        }

        if(n == -1)
        {
            shr_list = realloc(shr_list, sizeof(FileNode *) * (shr_list_len + 1));
            if(shr_list == NULL)
            {
                perrlog("realloc");
                return -1;
            }
            shr_list[shr_list_len] = new_list[i];
            last = shr_list_len;
            shr_list_len++;
        }
        else
        {
            free(shr_list[t]->file);
            free(shr_list[t]->BLAKE2b);
            free(shr_list[t]);
            shr_list[t] = new_list[n];
            last = t;
        }

        new_list = realloc(new_list, sizeof(FileNode *) * (new_list_len - 1));
        new_list_len--;
        if(new_list == NULL && new_list_len != 0)
        {
            perrlog("realloc");
            return -1;
        }

        shr_list[last]->BLAKE2b = malloc(TOX_FILE_ID_LENGTH);
        if(shr_list[last]->BLAKE2b == NULL)
        {
            perrlog("malloc");
            return -1;
        }

        hashing = last;
    }

    if(hashing >= 0)
    {
        rc = file_checksumcalc_noblock(shr_list[last]->BLAKE2b, shr_list[last]->file);
        if(rc <= 0)
        {
            hashing = -1;
            if(new_list_len == 0)
                file_recheck = false;

            /* execute callback for new file*/
            if(file_new_c != NULL)
                file_new_c(shr_list[last], last);
            yinfo("Hash: %i - %s", last, shr_list[last]->file);

            /* write filenode to cache */
            char cachepath[PATH_MAX + 20];
            snprintf(cachepath, sizeof(cachepath), "%s/%i", cachedir, last);
            filenode_dump(shr_list[last], cachepath);
        }

        if(rc < 0)
            return -1;
    }

    return 0;
}

void file_new_set_callback(void (*func)(FileNode *, int))
{
    file_new_c = func;
}

/* ENTERPRISE extern */
FileNode **file_get_shared(void)
{
    return shr_list;
}
int file_get_shared_len(void)
{
    return shr_list_len;
}

/*
 * file format:
 * put all predictable things in the beginning
 * put filename last and without newline
 * example:
 * BLAKE2b
 * mtime
 * size
 * file
 */
static FileNode *filenode_load(char *path)
{
    char BLAKE2b[TOX_FILE_ID_LENGTH * 2 +1], filename[PATH_MAX + 1];
    char *pos;

    FILE *fp = fopen(path, "rb");
    if(!fp)
    {
        perrlog("fopen");
        return NULL;
    }

    FileNode *fn = malloc(sizeof(FileNode));
    if(!fn)
    {
        perrlog("malloc");
        fclose(fp);
        return NULL;
    }

    fn->BLAKE2b = malloc(TOX_FILE_ID_LENGTH);
    if(!fn->BLAKE2b)
    {
        perrlog("malloc");
        fclose(fp);
        free(fn);
        return NULL;
    }

    if(fscanf(fp, "%64s\n", BLAKE2b) != 1) //TODO : variable format
        goto parserr;
    if(fscanf(fp, "%lld\n", (long long * ) &fn->mtime) != 1)
        goto parserr;
    if(fscanf(fp, "%lld\n", (long long * ) &fn->length) != 1)
        goto parserr;

    int rc = fread(filename, 1, PATH_MAX, fp);
    filename[rc] = '\0';

    if(ferror(fp))
    {
        perror("fread");
        goto parserr;
    }

    if(feof(fp))
    {
        uint8_t i;
        pos = BLAKE2b;
        // TODO : use sodium
        for (i = 0; i < TOX_FILE_ID_LENGTH; ++i, pos += 2)
            sscanf(pos, "%2hhx", &fn->BLAKE2b[i]);

        /* file_exists_shared will check later if they actually exist */
        fn->exists = false;

        fn->file = strdup(filename);
        if(fn->file == NULL)
        {
            perrlog("strdup");
            goto parserr;
        }
        fclose(fp);
        return fn;
    }

parserr:
    yerr("Error parsing: %s", path);
    fclose(fp);
    free(fn->BLAKE2b);
    free(fn);
    return NULL;
}

static int filenode_dump(FileNode *fnode, char *path)
{
    char *BLAKE2b;
    FILE *fp = fopen(path, "wb");
    if(fp == NULL)
    {
        perrlog("fopen");
        return -1;
    }

    BLAKE2b = human_readable_id(fnode->BLAKE2b, TOX_FILE_ID_LENGTH);

    fprintf(fp, "%s\n", BLAKE2b);
    fprintf(fp, "%lld\n", (long long) fnode->mtime);
    fprintf(fp, "%lld\n", (long long) fnode->length);
    fprintf(fp, "%s", fnode->file);

    free(BLAKE2b);

    if(fclose(fp) != 0)
    {
        perrlog("fclose");
        return -1;
    }
    return 0;
}

static int directory_count(const char *path)
{
    DIR * dirp;
    struct dirent * entry;
    int count = 0;
    uint8_t i;
    bool digitflag;

    dirp = opendir(path);
    if(dirp == NULL)
    {
        perrlog("opendir");
        return -1;
    }

    while((entry = readdir(dirp)) != NULL)
    {
        digitflag = true;
        if (entry->d_type == DT_REG) /* If the entry is a regular file */
        {
            /* check that there is no extraneus file in the cachedir */
            for(i=0; i<strlen(entry->d_name); i++)
                if(!isdigit(entry->d_name[i]))
                {
                    digitflag = false;
                    ywarn("Unknown file in cache dir, skipping: %s", entry->d_name);
                    break;
                }

            if(digitflag)
                count++;
        }
    }

    if(closedir(dirp) != 0)
    {
        perrlog("closedir");
        return -1;
    }

    return count;
}

int filenode_load_fromdir(const char *cachedir)
{
    FileNode *fn;
    char pathbuf[PATH_MAX + 20];
    int i = 0;

    int dircount = directory_count(cachedir);
    if(dircount == 0)
        return 0;
    if(dircount < 0)
        return -1;

    while(i < dircount)
    {
        snprintf(pathbuf, sizeof(pathbuf), "%s/%i", cachedir, i);
        ydebug("%i - %i", i, shr_list_len);
        if(access(pathbuf, R_OK|W_OK) != -1)
        {
            fn = filenode_load(pathbuf);
            if(fn)
            {
                shr_list = realloc(shr_list, sizeof(FileNode *) * (shr_list_len + 1));
                if(!shr_list)
                {
                    perrlog("realloc");
                    return -1;
                }
                shr_list[shr_list_len] = fn;
                shr_list_len++;
            }
        }

        i++;
    }

    return i;
}
