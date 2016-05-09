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

#include <time.h>
#include "friend.h"
#include "toxdata.h" // only for toxdata_set_shouldSaveConfig(true);
#include "ylog/ylog.h"

#include "tsfiles.h"
#include "file.h"

struct friend_info * friend_info_add_entry(struct list_head *friends_info, const uint32_t friend_number)
{
    struct list_head *ptr;
    struct friend_info *friend;
    struct friend_info *entry;

    friend = malloc(sizeof(struct friend_info));
    friend->friend_number = friend_number;
    friend->name = NULL;
    friend->status_message = NULL;

    list_for_each(ptr, friends_info) {
        entry = list_entry(ptr, struct friend_info, list);
        if (entry->friend_number > friend->friend_number) {
            list_add_tail(&friend->list, ptr);
            return friend;
        }
    }
    list_add_tail(&friend->list, friends_info);
    return friend;
}

void friend_info_destroy(struct friend_info *friend)
{
    list_del(&friend->list);
    free(friend->name);
    free(friend->status_message);
    free(friend);
}

void friend_info_update(const Tox *apptox, struct friend_info *friend)
{
    size_t name_size;
    size_t status_message_size;

    TOX_ERR_FRIEND_GET_PUBLIC_KEY error;
    TOX_ERR_FRIEND_QUERY error2;
    TOX_ERR_FRIEND_GET_LAST_ONLINE error3;

    friend->last_online = tox_friend_get_last_online(apptox, friend->friend_number, &error3);

    tox_friend_get_public_key(apptox, friend->friend_number, friend->tox_id_bin, &error);
    sodium_bin2hex(friend->tox_id_hex, sizeof(friend->tox_id_hex), friend->tox_id_bin, TOX_PUBLIC_KEY_SIZE);
    for (size_t i = 0; i < sizeof(friend->tox_id_hex) - 1; i++) {
        friend->tox_id_hex[i] = toupper(friend->tox_id_hex[i]);
    }

    name_size = tox_friend_get_name_size(apptox, friend->friend_number, &error2);
    friend->name = realloc(friend->name, name_size * sizeof(uint8_t) + 1);
    tox_friend_get_name(apptox, friend->friend_number, friend->name, &error2);
    friend->name[name_size] = '\0';

    status_message_size = tox_friend_get_status_message_size(apptox, friend->friend_number, &error2);
    friend->status_message = realloc(friend->status_message, status_message_size * sizeof(uint8_t) + 1);
    tox_friend_get_status_message(apptox, friend->friend_number, friend->status_message, &error2);
    friend->status_message[status_message_size] = '\0';

    friend->status = tox_friend_get_status(apptox, friend->friend_number, &error2);
    friend->connection_status = tox_friend_get_connection_status(apptox, friend->friend_number, &error2);
}

void friend_info_print(FILE *stream, struct friend_info *friend)
{
    //    char *date;
    char * stat;
    fprintf(stream, "Name : %s\n", friend->name );
    fprintf(stream, "Status message : %s\n", friend->status_message );
    fprintf(stream, "Tox Public key : %s\n", friend->tox_id_hex );
    //    fprintf(stream, "Last online : ", friend->last_online );
    if ( friend->status == TOX_USER_STATUS_NONE ) stat = "online";
    else if ( friend->status == TOX_USER_STATUS_AWAY ) stat = "away";
    else if ( friend->status == TOX_USER_STATUS_BUSY ) stat = "busy";
    else stat = "";
    fprintf(stream, "Status : %s\n", stat);
    //    fprintf(stream, "",connection_status );
}

/*
 * Callbacks
 */

void friend_request_cb(Tox *tox, const uint8_t *public_key, const uint8_t *message, size_t length, void *user_data)
{
    TOX_ERR_FRIEND_ADD err;
    uint32_t friend_number = tox_friend_add_norequest(tox, public_key, &err);
    struct list_head *f = user_data;
    yinfo("friends_info_number_of_friend = %d",friends_info_number_of_friend(f));

    if (err != TOX_ERR_FRIEND_ADD_OK) {
        yinfo("Could not add friend, error: %d", err);
    } else {
        friend_info_add_entry(user_data, friend_number);
        yinfo("Added to our friend list");
    }
}

void friend_name_cb(Tox *tox, uint32_t friend_number, const uint8_t *name, size_t length, void *user_data)
{
    struct friend_info *f;
    f = friend_info_by_friend_number(user_data, friend_number);
    f->name = realloc(f->name, length * sizeof(uint8_t) + 1);
    memcpy(f->name, name, length);
    f->name[length] = '\0';
    //    toxdata_set_shouldSaveConfig(true);
}

void friend_status_message_cb(Tox *tox, uint32_t friend_number, const uint8_t *message, size_t length,
                              void *user_data)
{
    struct friend_info *f;
    f = friend_info_by_friend_number(user_data, friend_number);
    f->status_message = realloc(f->status_message, length * sizeof(uint8_t) + 1);
    memcpy(f->status_message, message, length);
    f->status_message[length] = '\0';
    //    toxdata_set_shouldSaveConfig(true);
}

void friend_status_cb(Tox *tox, uint32_t friend_number, TOX_USER_STATUS status, void *user_data)
{
    struct friend_info *f;
    f = friend_info_by_friend_number(user_data, friend_number);
    f->status = status;
    //    toxdata_set_shouldSaveConfig(true);
}



/*
 * Friends functions.
 *
 *
 */

void friends_info_print(FILE *stream, struct list_head *friends_info)
{
    char *date;
    struct friend_info *f;
    size_t friend_list_size = friends_info_number_of_friend(friends_info);

    fprintf(stream, "| %3zu | %-64s | %-20s | %-90s | %-24s | S | C |\n",
            friend_list_size, "< Public Key >", "<name (truncated)>", "<status message (truncated)>", "<last online>");
    fprintf(stream, "|----:|------------------------------------------------------------------|"
                    "----------------------|--------------------------------------------------------------------------------------------|"
                    ":------------------------:|---|---|\n");

    list_for_each_entry(f, friends_info, list) {
        fprintf(stream, "| %3d", f->friend_number);
        fprintf(stream, " | %-64s", f->tox_id_hex);
        fprintf(stream, " | %-20s", (char*)f->name);
        fprintf(stream, " | %-90s", (char*)f->status_message);

        if (f->last_online) {
            date = strdup(ctime((time_t*)&f->last_online));
            date[strlen(date) - 1] = '\0';
            fprintf(stream, " | %s", date);
            free(date);
        } else fprintf(stream, " | %-24s", "");
        char stat;

        if ( f->status == TOX_USER_STATUS_NONE ) stat = 'O';
        else if ( f->status == TOX_USER_STATUS_AWAY ) stat = 'X';
        else if ( f->status == TOX_USER_STATUS_BUSY ) stat = '/';
        else stat = f->status;
        fprintf(stream, " | %c", stat);


        if ( f->connection_status == TOX_CONNECTION_NONE ) stat = 'X';
        else stat = '0';
        fprintf(stream, " | %c", stat);

        fprintf(stream, " | \n");
    }
}

void print_friends_pubkeys(FILE *stream, struct list_head *friends_info)
{
    struct friend_info *f;
    list_for_each_entry(f, friends_info, list) {
        fprintf(stream, "%s\n", f->tox_id_hex);
    }
}

void friends_info_update(const Tox *apptox, struct list_head *friends_info)
{
    struct friend_info *f;

    list_for_each_entry(f, friends_info, list) {
        friend_info_update(apptox, f);
    }
}

void friends_info_init(const Tox *apptox, struct list_head *friends_info, size_t *friend_list_size)
{
    uint32_t *friend_list;

    *friend_list_size = tox_self_get_friend_list_size(apptox);
    friend_list = malloc( *friend_list_size * sizeof(uint32_t));
    tox_self_get_friend_list(apptox, friend_list);

    for (size_t i = 0; i < *friend_list_size; i++) {
        friend_info_add_entry(friends_info, friend_list[i]);
    }
    free(friend_list);
    friends_info_update(apptox, friends_info);
}

/* Getters
 *
 *
 */

struct friend_info *friend_info_by_public_keyhex(struct list_head *friends_info, const char *public_key)
{
    struct friend_info *f;
    list_for_each_entry(f, friends_info, list) {
        if (!strncmp(f->tox_id_hex,public_key,TOX_PUBLIC_KEY_SIZE * 2))
        {
            return f;
        }
    }
    return NULL;
}

struct friend_info *friend_info_by_public_keybin(struct list_head *friends_info, const uint8_t *public_key)
{
    struct friend_info *f;
    list_for_each_entry(f, friends_info, list) {
        if (!sodium_memcmp(f->tox_id_bin,public_key,TOX_PUBLIC_KEY_SIZE))
        {
            return f;
        }
    }
    return NULL;
}


struct friend_info *friend_info_by_friend_number(struct list_head *friends_info, const uint32_t friend_number)
{
    struct friend_info *f;

    list_for_each_entry(f, friends_info, list) {
        if (f->friend_number == friend_number)
        {
            return f;
        }
    }
    return NULL;
}

/* lack of size() in list.h ?
 *
 */
int friends_info_number_of_friend(struct list_head *friends_info)
{
    struct friend_info *f;
    int i = 0;
    list_for_each_entry(f, friends_info, list) {
        i++;
    }
    return i;
}

void friends_info_destroy(struct list_head *friends_info)
{
    struct friend_info *f, *n;

    list_for_each_entry_safe(f, n, friends_info, list) {
        friend_info_destroy(f);
    }
}

/* Misc : friends filters.
 *
 */

//void basic_antispam(Tox *tox, uint64_t timeout, uint64_t current_time, struct list_head *friends_info)
//{
//    struct friend_info *f, *n;

//    list_for_each_entry_safe(f, n, friends_info, list) {
//        if ( current_time - f->last_online > timeout )
//            && ( strlen(f->name) == 0 ) // srly, anonymous ?
//            continue
//        yinfo("removing friend %s", f->name);
//        tox_friend_delete(tox, f->friend_number, NULL);
//        friend_info_destroy(f);
//    }
//}

// TODO : f->last_online should be UINT64_MAX weird (in print too)
void friend_cleanup(Tox *tox, uint64_t timeout, uint64_t current_time, struct list_head *friends_info)
{
    struct friend_info *f, *n;

    list_for_each_entry_safe(f, n, friends_info, list) {
        if (f->last_online == UINT64_MAX) // TODO, at this point, we keep not replying friend.
            continue;
        if (current_time - f->last_online < timeout)
            continue;
        yinfo("removing friend %s", f->name);
        tox_friend_delete(tox, f->friend_number, NULL);
        friend_info_destroy(f);
    }
}
