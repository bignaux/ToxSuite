#include "tsfiles.h"
#include "ylog/ylog.h"
#include "bencode/bencode.h"

#include <stdio.h>

int save_senders(struct list_head* FileQueue, struct list_head *friends_info)
{
    struct FileSender* f;
    struct friend_info *fr;
    char msg[5000];
    char *tmp;
    static int prevsz = 0;
    static char *prevmsg;
    int blen;
    be_node *whole, *toxid, *path, *name;
    FILE *file;

    ydebug("entering save_senders");
    whole = be_create_dict();
    list_for_each_entry(f, FileQueue, list)
    {
        if (!f->pathname || !f->filename)
            break;
        fr = friend_info_by_friend_number(friends_info, f->friend_number);
        if (!fr)
            break;
        ytrace("fr->friend_number = %d , f->friend_number = %d", fr->friend_number, f->friend_number);
        ytrace("fr->tox_id_hex : %s , %zu", fr->tox_id_hex, strlen(fr->tox_id_hex));
        toxid = be_create_str_wlen(fr->tox_id_hex, strlen(fr->tox_id_hex));
        name = be_create_str_wlen(f->filename, strlen(f->filename));
        path = be_create_str_wlen(f->pathname, strlen(f->pathname));
        be_add_keypair(whole, "toxid", toxid);
        be_add_keypair(whole, "name", name);
        be_add_keypair(whole, "path", path);
    }
    blen = be_encode(whole, msg, 5000);
    be_free(whole);

    //    if(blen != prevsz) {
    //        prevsz = blen;
    //        tmp = realloc(prevmsg, blen);
    //        if(!tmp)
    //            return -1;
    //        prevmsg = tmp;
    //    } else if(!strncmp(msg, prevmsg, blen))
    //       return 0;

    //    memcpy(prevmsg,msg,blen);
    if(blen > 2) {
        ydebug("%d : %s", blen, msg);
        file = fopen("savesender.dat","w");
        if (!file) {
            yerr("can't save savesender.dat");
            return -1;
        }
        fwrite(msg, blen, 1, file);
        fclose(file);
    }
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
        ydebug("resume %s for friend %s", f->filename, fr->name);
        add_filesender(tox, f);
    }
    return 0;
}


int load_senders(struct list_head* FileQueue, struct list_head *friends_info)
{
    FileSender *f;
    FILE *file;
    char *msg;
    be_node *whole, *toxid, *path, *name;
    uint64_t filesize;
    struct friend_info *fr;
    //    char public_key[64];
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
    fread(msg, filesize, 1, file);
    fclose(file);

    if (filesize < 2)
        return 0;

    f = FileSender_new(FileQueue);
    whole = be_decoden(msg, filesize);
    if (!whole) {
        yerr("load_senders can't decode savesender.dat.");
        return -1;
    }

#ifdef BE_DEBUG_DECODE
    be_dump(whole);
#endif

    for (i = 0; whole->val.d[i].val; ++i)
    {
        switch(i) {
        case 0:
            toxid = whole->val.d[i].val;
            fr = friend_info_by_public_key(friends_info,toxid->val.s);
            ydebug("load_senders: fr->friend_number %d", fr->friend_number);
            f->friend_number = fr->friend_number;
            ydebug("fn=%d tox:%s",f->friend_number, toxid->val.s);
            break;
        case 1:
            path = whole->val.d[i].val;
            ydebug("%s", path->val.s);
            f->pathname = strdup(path->val.s);
            break;
        case 2:
            name = whole->val.d[i].val;
            ydebug("%s", name->val.s);
            f->filename = strdup(name->val.s);
            break;
        }
    }
    be_free(whole);
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
