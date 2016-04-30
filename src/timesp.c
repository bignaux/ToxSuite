#include "suit.h"
#include "call.h"
#include "command.h"
#include "file.h"
#include "toxdata.h"
#include "timespec.h"
#include "unused.h"

#include "ylog/ylog.h"
#include "jsmn/jsmn.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

#include <sodium.h>

static bool signal_exit = false;

#define VERSION "0.0.1"
#define FRIEND_PURGE_INTERVAL 3600
#define GROUP_PURGE_INTERVAL 3600


void friend_cleanup(Tox *tox)
{
    uint32_t friend_count = tox_self_get_friend_list_size(tox);

    if (friend_count == 0) {
        return;
    }

    uint32_t friends[friend_count];
    tox_self_get_friend_list(tox, friends);

    uint64_t curr_time = time(NULL);
    for (uint32_t i = 0; i < friend_count; i++) {
        TOX_ERR_FRIEND_GET_LAST_ONLINE err;
        uint32_t friend = friends[i];
        uint64_t last_online = tox_friend_get_last_online(tox, friend, &err);

        if (err != TOX_ERR_FRIEND_GET_LAST_ONLINE_OK) {
            yerr("couldn't obtain 'last online', this should never happen\n");
            continue;
        }

        if (curr_time - last_online > 2629743) {
            yinfo("removing friend %d\n", friend);
            tox_friend_delete(tox, friend, NULL);
        }
    }
}

//bool load_json_profile(Tox **tox, struct Tox_Options *options)
//{
//    FILE *file = fopen("config.json", "rb");
//    jsmn_parser parser;
//    jsmntok_t tokens[256];

//    char keyname[50];
////    size_t szkey = 0;


//    char *line = NULL;
//    char *js = NULL;
//    size_t len = 0;
//    size_t blen = 0;
//    size_t offset = 0;
//    int read, r = 0;
//    int j = 0, i = 0;

//    char ** masterkeys = NULL;

//    //    enum jsmnerr {
//    //        /* Not enough tokens were provided */
//    //        JSMN_ERROR_NOMEM = -1,
//    //        /* Invalid character inside JSON string */
//    //        JSMN_ERROR_INVAL = -2,
//    //        /* The string is not a full JSON packet, more bytes expected */
//    //        JSMN_ERROR_PART = -3
//    //    };


//    jsmn_init(&parser);


//    if (file) {

//        /* TODO : improve parsing against jsmn_parse()
//         *
//         */
//        yinfo("read a json bloc");
//        // read a json bloc
//        while ( (read = getline(&line, &len, file)) != -1 ) {
//            offset = blen;
//            blen += read;
//            js = realloc(js, blen);
//            strncpy(js + offset, line, read);
//        }
//        free(line);

//        r = jsmn_parse(&parser, js, blen, tokens, 256);
//        while ( i < r ) {
//            if ( tokens[i].type == JSMN_PRIMITIVE ) {
//                strncpy(keyname, js, tokens[i].end - tokens[i].start + 1);
//                keyname[tokens[i].end - tokens[i].start ] = '\0';
//                yinfo("key:%s", keyname);
//            }else if ( tokens[i].type == JSMN_ARRAY ) {
//                masterkeys = realloc(masterkeys, sizeof(char*) * tokens[i].size); // resize array of strings
//            }else if ( tokens[i].type == JSMN_STRING) {
//                masterkeys[j] = realloc( masterkeys[j],  tokens[i].end - tokens[i].start );
//                strncpy( masterkeys[j], js + tokens[i].start, tokens[i].end - tokens[i].start );
//                j++;
//            }
//            i++;
//        }
//        fclose(file);
//    }

//    for (int k = 0; k < j; k++) {
//        yinfo("%d keys : %s", k, masterkeys[k]);
//    }
//    return false;
//}

void self_connection_status(Tox *UNUSED(tox), TOX_CONNECTION status, void *UNUSED(userData))
{
    if (status == TOX_CONNECTION_NONE) {
        yinfo("Lost connection to the tox network");
    } else {
        yinfo("Connected to the tox network, status: %d", status);
    }
}

static void handle_signal(int UNUSED(sig))
{
    signal_exit = true;
}

struct suit_info *suit_info_new(struct suit_info *si)
{
    if (!si)
        si = malloc(sizeof(struct suit_info));
    si->tox = NULL;
    si->toxav = NULL;
    si->start_time = time(NULL);
    si->data_filename = strdup("data.tox");
    si->last_purge = 0;
    INIT_LIST_HEAD(&si->friends_info);
    ydebug("suit_info initialised");
    return si;
}

#include <stdarg.h>

// this function returns minimum of integer numbers passed.  First
// argument is count of numbers.
int min(int arg_count, ...)
{
    int i;
    int min, a;

    // va_list is a type to hold information about variable arguments
    va_list ap;

    // va_start must be called before accessing variable argument list
    va_start(ap, arg_count);

    // Now arguments can be accessed one by one using va_arg macro
    // Initialize min as first argument in list
    min = va_arg(ap, int);

    // traverse rest of the arguments to find out minimum
    for(i = 2; i <= arg_count; i++) {
        if((a = va_arg(ap, int)) < min)
            min = a;
    }

    //va_end should be executed before the function returns whenever
    // va_start has been previously used in that function
    va_end(ap);

    return min;
}

int main(int argc, char **argv)
{
    ylog_set_level(YLOG_DEBUG, getenv("YLOG_LEVEL"));

    uint64_t tox_delay = 0,
            toxav_delay = 0,
            calltest_delay = 0,
            sleep_delay = 0;

    int i = 10;

    // initial to timespec_now(&tp1);?
    struct timespec tpnow;
    struct timespec tsprev;
    struct timespec tselasped = { 0, 0 };

    while(--i) {
        timespec_now(&tpnow);
        memcpy(&tsprev, &tpnow, sizeof(struct timespec));
        sleep(1);
        timespec_now(&tpnow);
        timespec_subtract(&tselasped, &tpnow, &tsprev); // elasped time
        sleep_delay = timespec_to_ns(&tselasped);
        ydebug("elasped time { %ju, %ju} %ju",tselasped.tv_sec, tselasped.tv_nsec, sleep_delay);
    }

    return 0;
}
