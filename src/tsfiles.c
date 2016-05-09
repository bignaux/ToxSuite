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
#include "misc.h"

#include <stdio.h>

#include "fileop.h" // file_get_shared

void encode_FileNode(be_node *info, const FileNode *fn)
{
    be_node *name, *hash, *length;
    char *blake;

    name = be_create_str_wlen(fn->file, strlen(fn->file));
    length = be_create_int(fn->length);
    blake = human_readable_id(fn->BLAKE2b, TOX_FILE_ID_LENGTH);
    hash = be_create_str_wlen(blake, strlen(blake));
    be_add_keypair(info, "name", name);
    be_add_keypair(info, "length", length);
    be_add_keypair(info, "hash", hash);
    free(blake);
}

int NumDigits(int x)
{
    x = abs(x);
    return (x < 10 ? 1 :
        (x < 100 ? 2 :
        (x < 1000 ? 3 :
        (x < 10000 ? 4 :
        (x < 100000 ? 5 :
        (x < 1000000 ? 6 :
        (x < 10000000 ? 7 :
        (x < 100000000 ? 8 :
        (x < 1000000000 ? 9 :
        10)))))))));
}

// approximation
// TODO add sign support and long long int.
size_t be_size(be_node *node)
{
    size_t counter = 0;
    size_t tmp;

    switch(node->type) {
    case BE_STR:
        tmp = strlen(node->val.s);
        counter += tmp + NumDigits(tmp) + 1; // :
        break;
    case BE_INT:
        counter += NumDigits(node->val.i) + 2; // ie
        break;
    case BE_LIST:
        for(int i=0; node->val.l[i]; i++) {
            counter += be_size(node->val.l[i]);
        }
        counter += 2; // le
        break;
    case BE_DICT:
        for(int i=0; node->val.d[i].val; i++) {
            counter += be_size(node->val.d[i].val);
            tmp = strlen(node->val.d[i].key);
            counter += tmp + NumDigits(tmp) + 1; // :
        }
        counter += 2; // de
        break;
    }
    return counter;
}

// data to return to friends.
void dump_shrlist()
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
    blen = be_size(ROOT) +1;  // 'why +1 ? '\0' ?
    ydebug("be_size = %d", blen);
    msg = malloc(blen + 1);
    blen = be_encode(ROOT, msg, blen);
    ydebug("be_encode = %d", blen);
    be_free(ROOT);

    if(blen > 2) {
        ydebug("%d : %s", blen, msg);
        file = fopen("savfileshr.dat","w");
        if (!file) {
            yerr("can't save savfileshr.dat");
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
        ytrace("fr->tox_id_hex : %s , %zu", fr->tox_id_hex, strlen(fr->tox_id_hex));
        toxid = be_create_str_wlen(fr->tox_id_hex, strlen(fr->tox_id_hex));
        path = be_create_str_wlen(f->pathname, strlen(f->pathname));
        info = be_create_dict();
        encode_FileNode(info, fn);
        be_add_keypair(filesend, "toxid", toxid);
        be_add_keypair(filesend, "path", path);
        be_add_keypair(filesend, "info", info);
        be_add_list(ROOT, filesend);

    }
    blen = be_size(ROOT) +1;
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
            ydebug("%s", hash->val.s);
            f->BLAKE2b = (uint8_t*) strdup(hash->val.s);
        }
    }
}

int load_senders(struct list_head* FileQueue, struct list_head *friends_info)
{
    FileSender *f;
    FILE *file;
    char *msg;
    be_node *ROOT, *filesend, *toxid, *info, *path;
    int64_t filesize;
    struct friend_info *fr;
    int i;

    file = fopen("savesender.dat", "r");
    if(!file) {
        ywarn("Can't open savesender.dat.");
        return -1;
    }

    fseek(file, 0, SEEK_END);
    filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    msg = malloc(filesize);
    if(fread(msg, filesize, 1, file) < 1) {
        yerr("Can't read savesender.dat.");
        free(msg);
        fclose(file);
        return -1;
    }

    fclose(file);
    ROOT = be_decoden(msg, filesize);
    free(msg);

    if (!ROOT) {
        yerr("load_senders can't decode savesender.dat.");
        return -1;
    }

#ifdef BE_DEBUG_DECODE
    be_dump(ROOT);
#endif

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
                fr = friend_info_by_public_key(friends_info,toxid->val.s);
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
