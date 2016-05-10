/*    Copyright (C) 2016  Ronan Bignaux
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

/* read https://github.com/irungentoo/Tox_Client_Guidelines/blob/master/Important/File_Transfers.md
 *
 */

#include "tsfiles.h"
#include "ylog/ylog.h"
#include "bencode/bencode.h"
//#include "misc.h"

#include <stdio.h>
//#include <string.h> // rawmemchr
#include "fileop.h" // file_get_shared

void encode_FileNode(be_node *info, const FileNode *fn)
{
    be_node *name, *hash, *length;

    name = be_create_str_wlen(fn->file, strlen(fn->file));
    length = be_create_int(fn->length);
    hash = be_create_str_wlen((char *)fn->BLAKE2b, TOX_FILE_ID_LENGTH);
    be_add_keypair(info, "name", name);
    be_add_keypair(info, "length", length);
    be_add_keypair(info, "hash", hash);
}

void decode_FileNode(FileNode *f, const be_node *info)
{
    be_node *name, *hash, *length;
    for(int j=0; info->val.d[j].val;j++)
    {
        if(!strcmp("name",info->val.d[j].key)) {
            name = info->val.d[j].val;
            ydebug("%s", name->val.s);
            f->file = strdup(name->val.s);
        } else if (!strcmp("length",info->val.d[j].key)) {
            length = info->val.d[j].val;
            ydebug("%lld", length->val.i);
            f->length = length->val.i;
        } else if (!strcmp("hash",info->val.d[j].key)) {
            hash = info->val.d[j].val;
            f->BLAKE2b = malloc(TOX_FILE_ID_LENGTH);
            memcpy(f->BLAKE2b, hash->val.s, TOX_FILE_ID_LENGTH);
        }
    }
}

//not very performant but do the job
long long int NumDigits(long long int x)
{
    char buffer[22];
    return sprintf(buffer, "%lld", x);
}

size_t be_node_len(be_node *node)
{
    size_t counter = 0;
    size_t tmp;

    switch(node->type) {
    case BE_STR:
        tmp = be_str_len(node);
        counter += tmp + NumDigits(tmp) + 1; // :
        break;
    case BE_INT:
        counter += NumDigits(node->val.i) + 2; // ie
        break;
    case BE_LIST:
        for(int i=0; node->val.l[i]; i++) {
            counter += be_node_len(node->val.l[i]);
        }
        counter += 2; // le
        break;
    case BE_DICT:
        for(int i=0; node->val.d[i].val; i++) {
            counter += be_node_len(node->val.d[i].val);
            tmp = strlen(node->val.d[i].key);
            counter += tmp + NumDigits(tmp) + 1; // :
        }
        counter += 2; // de
        break;
    }
    return counter;
}

be_node *be_node_loadfile(const char *filename)
{
    FILE *file;
    char *msg;
    int64_t filesize;
    be_node *ROOT;

    file = fopen(filename, "r");
    if(!file) {
        ywarn("Can't open %s.", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    msg = malloc(filesize);
    if(fread(msg, filesize, 1, file) < 1) {
        yerr("Can't read %s.", filename);
        free(msg);
        fclose(file);
        return NULL;
    }

    fclose(file);
    ROOT = be_decoden(msg, filesize);
    free(msg);

    if (!ROOT) {
        yerr("be_node_loadfile can't decode %s.", filename);
        return NULL;
    }

#ifdef BE_DEBUG_DECODE
    be_dump(ROOT);
#endif
    return ROOT;
}

int load_shrlist(const char *filename)
{
    FileNode **shr_list = file_get_shared();
    int shr_list_len = file_get_shared_len();
    be_node *ROOT;

    ROOT = be_node_loadfile(filename);
    if (!ROOT)
        return -1;

    ydebug("load_shrlist, shr_list_len=%d",shr_list_len);
    for (int k=0; ROOT->val.l[k]; ++k)
    {
        shr_list = realloc(shr_list, sizeof(FileNode *) * (shr_list_len + 1));
        if(!shr_list)
        {
            yerr("realloc");
            return -1;
        }
        shr_list[shr_list_len] = malloc(sizeof(struct FileNode));
        decode_FileNode(shr_list[shr_list_len], ROOT->val.l[k]); // +1?
        shr_list_len++; //can't set outside fileop
    }
    be_free(ROOT);
    return 0;
}

// data to return to friends.
void dump_shrlist(const char*filename)
{
    FileNode *fn, **shrlist = file_get_shared();
    int blen, shrlen = file_get_shared_len();
    be_node *ROOT, *info;
    char *msg;
    FILE *file;

    ROOT = be_create_list();
    for(int i=0; i<shrlen; i++)
    {
        fn = shrlist[i];
        info = be_create_dict();
        encode_FileNode(info, fn);
        be_add_list(ROOT, info);
    }
    blen = be_node_len(ROOT) +1;  // 'why +1 ? '\0' ?
    ydebug("be_size = %d", blen);
    msg = malloc(blen);
    blen = be_encode(ROOT, msg, blen);
    ydebug("be_encode = %d", blen);
    be_free(ROOT);

    if(blen > 2) {
        file = fopen(filename,"w");
        if (!file) {
            yerr("can't save %s", filename);
            return;
        }
        fwrite(msg, blen, 1, file);
        fclose(file);
    }
    free(msg);
}

int save_senders(struct list_head* FileQueue, struct list_head *friends_info)
{
    struct FileSender* f;
    struct friend_info *fr;
    struct FileNode *fn;
    char *msg;

    int blen;
    be_node *ROOT, *filesend, *toxid, *info, *path;
    FILE *file;

    ydebug("entering save_senders");
    ROOT = be_create_list();
    list_for_each_entry(f, FileQueue, list)
    {
        // TODO : check info->file ?
        if (!f->pathname)
            break;
        fn = f->info;
        fr = friend_info_by_friend_number(friends_info, f->friend_number);
        if (!fr)
            break;
        filesend = be_create_dict();
        ytrace("fr->friend_number = %d , f->friend_number = %d", fr->friend_number, f->friend_number);
        //        ytrace("fr->tox_id_hex : %s , %zu", fr->tox_id_hex, strlen(fr->tox_id_hex));
        //        toxid = be_create_str_wlen(fr->tox_id_hex, strlen(fr->tox_id_hex));
        toxid = be_create_str_wlen((char *)fr->tox_id_bin, TOX_PUBLIC_KEY_SIZE);
        path = be_create_str_wlen(f->pathname, strlen(f->pathname));
        info = be_create_dict();
        encode_FileNode(info, fn);
        be_add_keypair(filesend, "toxid", toxid);
        be_add_keypair(filesend, "path", path);
        be_add_keypair(filesend, "info", info);
        be_add_list(ROOT, filesend);

    }
    blen = be_node_len(ROOT) +1;
    msg = malloc(blen);
    blen = be_encode(ROOT, msg, blen);
    be_free(ROOT);

    if(blen > 2) {
        ydebug("%d : %s", blen, msg);
        file = fopen("savesender.dat","w");
        if (!file) {
            yerr("can't save savesender.dat");
            return -1;
        }
        fwrite(msg, blen, 1, file);
        fclose(file);
    } else unlink("savesender.dat");

    free(msg);
    return 0;
}

int resume_send(Tox *tox, struct list_head* FileQueueSend, struct list_head* FileQueueLoad, struct friend_info *fr)
{
    struct FileSender *f, *n;

    list_for_each_entry_safe(f, n, FileQueueLoad, list)
    {
        if(f->friend_number != fr->friend_number)
            break;
        list_del(&f->list);
        list_add(&f->list, FileQueueSend);
        ydebug("resume %s for friend %s", f->info->file, fr->name);
        add_filesender(tox, f);
    }
    return 0;
}

int load_senders(struct list_head* FileQueue, struct list_head *friends_info)
{
    FileSender *f;
    be_node *ROOT, *filesend, *toxid, *info, *path;

    struct friend_info *fr;
    int i;

    ROOT = be_node_loadfile("savesender.dat");
    if (!ROOT)
        return -1;

    char *key;
    for (int k=0; ROOT->val.l[k]; ++k)
    {
        if(ROOT->val.l[k]->type != BE_DICT)
            break;
        f = FileSender_new(FileQueue);
        filesend = ROOT->val.l[k];
        for (i = 0; filesend->val.d[i].val; ++i)
        {
            key = filesend->val.d[i].key;
            if (!strcmp("toxid",key)) {
                toxid = filesend->val.d[i].val;

                fr = friend_info_by_public_keybin(friends_info,(uint8_t *) toxid->val.s);
                if(!fr) {
                    FileSender_destroy(f);
                    break;
                }
                ydebug("load_senders: fr->friend_number %d", fr->friend_number);
                f->friend_number = fr->friend_number;
                ydebug("fn=%d tox:%s",f->friend_number, toxid->val.s);
            } else if (!strcmp("path",key)) {
                path = filesend->val.d[i].val;
                ydebug("%s", path->val.s);
                f->pathname = strdup(path->val.s);
            } else if (!strcmp("info",key)) {
                info = filesend->val.d[i].val;
                decode_FileNode(f->info, info);
            }
        }
    }
    // TODO break when data are incomplete ?
    be_free(ROOT);
    return 0;
}

void friend_connection_status_cb(Tox *tox, uint32_t friend_number, TOX_CONNECTION connection_status,
                                 void *user_data)
{
    struct friend_info *f;
    f = friend_info_by_friend_number(user_data, friend_number);
    f->connection_status = connection_status;
    switch(connection_status)
    {
    case TOX_CONNECTION_NONE:
        break;
    case TOX_CONNECTION_TCP:
    case TOX_CONNECTION_UDP:
        ytrace("%s is connected via %s.", f->name, (f->connection_status==TOX_CONNECTION_TCP) ? "tcp" : "udp" );
        ytrace("%s tox:%s", f->name, f->tox_id_hex);
        resume_send(tox, &FilesSender, &FileQueueLoaded, f);
        break;
    }
    //    toxdata_set_shouldSaveConfig(true);
}
