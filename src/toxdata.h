#ifndef _TOX_PROFILE_
#define _TOX_PROFILE_


#include "friend.h"
#include "list.h"

bool file_exists(const char *filename);
bool toxdata_get_shouldSaveConfig(void);
void toxdata_set_shouldSaveConfig(bool saved);

int load_profile(Tox **tox, struct Tox_Options *options, const char *data_filename, const char *passphrase);
int save_profile(Tox *tox, const char *filename, const char *passphrase);

int export_keys(const char *filename, struct list_head *friends_info);
int import_keys(Tox *tox,struct list_head *friends_info, const char *filename);
int remove_keys(Tox *tox, struct list_head *friends_info, const char *filename);

#endif
