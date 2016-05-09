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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tox/tox.h>

#include "filesend.h"
#include "ylog/ylog.h"

#include "file.h"

char packlist_filename[] = "packlist.txt";

static FILE *file_list_create(void)
{
    FileNode **fnode = file_get_shared();
    int fnode_len = file_get_shared_len();
    char hu_size[8];
    int i, n = 0, packs_avail = 0;

    FILE *fp = fopen(packlist_filename, "w");
    if(fp == NULL)
    {
        perrlog("fopen");
        return NULL;
    }

    n = FileQueue_size(&FilesSender);

    for(i = 0; i < fnode_len; i++)
        if(fnode[i]->exists)
            packs_avail++;

    fprintf(fp, "** %i Packs ** %i/%i Slots open **\n", packs_avail, NUM_FILE_SENDERS - n, NUM_FILE_SENDERS);
    fprintf(fp, "** #pack_num  [size]  filename **\n");
    for(i = 0; i < fnode_len; i++)
    {
        if(fnode[i]->exists)
        {
            human_readable_filesize(hu_size, fnode[i]->length);
            fprintf(fp, "#%-10i [%s] %s\n", i, hu_size, gnu_basename(fnode[i]->file));
        }
    }

    return fp;
}

int file_sender_new(uint32_t friend_number, FileNode **shrfiles, int packn, Tox *m)
{
    struct FileSender *f = FileSender_new(&FilesSender);

    if(packn < 0)
    {
        /* create packlist */
        f->file = file_list_create();
        if(!f->file) {
            FileSender_destroy(f);
            return FILESEND_ERR_FILEIO;
        }
        f->info->file = strdup(packlist_filename);
        f->pathname = strdup(packlist_filename);
    }
    else
    {
        /* use existing file */
        ytrace("copy f->info from shrfiles");
        memcpy(f->info, shrfiles[packn], sizeof(struct FileNode));
        memcpy(f->info->BLAKE2b, shrfiles[packn]->BLAKE2b, TOX_FILE_ID_LENGTH);
        f->info->file = strdup(gnu_basename(shrfiles[packn]->file));
        f->pathname = strdup(shrfiles[packn]->file);
        f->file = fopen(f->pathname, "r");
        if (f->file == NULL)
        {
            FileSender_destroy(f);
            perrlog("fopen");
            return FILESEND_ERR_FILEIO;
        }
    }
    f->friend_number = friend_number;
    add_filesender(m, f);
    if (f->file_number == UINT32_MAX)
    {
        return FILESEND_ERR_SENDING;
    }
    return FILESEND_OK;
}
