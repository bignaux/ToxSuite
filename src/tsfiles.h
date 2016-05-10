#ifndef _TSFILES_
#define _TSFILES_

#include "file.h"
#include "friend.h"
#include "list.h"
#include <tox/tox.h>


struct list_head FileQueueLoaded;
void dump_shrlist(const char*filename);
int load_shrlist(const char *filename);

//void encode_FileNode(be_node *info, const FileNode *fn);
//void decode_FileNode(FileNode *f, const be_node *info);
int save_senders(struct list_head* FileQueue, struct list_head *friends_info);
int resume_send(Tox *tox, struct list_head* FileQueueSend, struct list_head* FileQueueLoad, struct friend_info *fr);
int load_senders(struct list_head* FileQueue, struct list_head *friends_info);
//refactoring and move in friend.h
void friend_connection_status_cb(Tox *tox, uint32_t friend_number, TOX_CONNECTION connection_status,
        void *user_data);

#endif
