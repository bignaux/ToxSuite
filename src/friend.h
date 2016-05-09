#ifndef _FRIEND_H
#define _FRIEND_H

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <tox/tox.h>
#include <tox/toxav.h>
#include <tox/toxencryptsave.h>
#include <sodium.h>

#include "list.h"

/* TODO :
 * - verify that iteration can't be brocken by list removal.
 * - test friend remove.
 *
 *
 *
 */

struct friend_info {

    /* duplicate core infos */
    uint32_t friend_number;
    uint8_t *name;
    uint8_t *status_message;
    uint8_t tox_id_bin[TOX_PUBLIC_KEY_SIZE];
    uint64_t last_online;
    TOX_USER_STATUS status;
    TOX_CONNECTION connection_status;

    /* custom */
    char tox_id_hex[TOX_PUBLIC_KEY_SIZE * 2 + 1];
    struct list_head list;
};

struct friend_info *friend_info_add_entry(struct list_head *friends_info, const uint32_t friend_number);
struct friend_info *friend_info_by_public_keybin(struct list_head *friends_info, const uint8_t *public_key); // prefer this!
struct friend_info *friend_info_by_public_keyhex(struct list_head *friends_info, const char*public_key);
struct friend_info *friend_info_by_friend_number(struct list_head *friends_info, const uint32_t friend_number);

void friend_info_destroy(struct friend_info *friend);
void friend_info_update(const Tox *apptox, struct friend_info *friend);
void friend_info_print(FILE *stream, struct friend_info *friend);

void friends_info_destroy(struct list_head *friends_info);
void friends_info_init(const Tox *apptox, struct list_head *friends_info, size_t *friend_list_size);
void friends_info_print(FILE *stream, struct list_head *friends_info);
void friends_info_update(const Tox *apptox, struct list_head *friends_info);
void print_friends_pubkeys(FILE *stream, struct list_head *friends_info);
int friends_info_number_of_friend(struct list_head *friends_info);

/* predefined callbacks */
void friend_request_cb(Tox *tox, const uint8_t *public_key, const uint8_t *message, size_t length, void *user_data);
void friend_name_cb(Tox *tox, uint32_t friend_number, const uint8_t *name, size_t length, void *user_data);
void friend_status_cb(Tox *tox, uint32_t friend_number, TOX_USER_STATUS status, void *user_data);

void friend_status_message_cb(Tox *tox, uint32_t friend_number, const uint8_t *message, size_t length,
        void *user_data);

/* Misc */
void friend_cleanup(Tox *tox, uint64_t timeout, uint64_t current_time, struct list_head *friends_info);

#endif
