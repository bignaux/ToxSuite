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

    FILE *fp = tmpfile();
    if(fp == NULL)
    {
        perrlog("tmpfile");
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
            human_readable_filesize(hu_size, fnode[i]->size);
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
        if(!f->file)
            return FILESEND_ERR_FILEIO; //todo : destroyer
        f->filename = packlist_filename;
        f->pathname = packlist_filename; //todo
    }
    else
    {
        /* use existing file */
        f->details = shrfiles[packn];
        f->file = fopen(f->details->file, "r");
        if (f->file == NULL)
        {
            perrlog("fopen");
            return FILESEND_ERR_FILEIO;
        }

        f->file_size = (uint64_t) f->details->size;
        f->filename = gnu_basename(f->details->file);
        f->pathname = f->details->file;
    }
    f->friend_number = friend_number;
    add_filesender(m, f);
    if (f->file_number == UINT32_MAX)
    {
        return FILESEND_ERR_SENDING;
    }
    return FILESEND_OK;
}
