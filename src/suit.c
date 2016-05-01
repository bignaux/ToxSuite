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

#include "suit.h"
#include "call.h"
#include "command.h"
#include "file.h"
#include "toxdata.h"
#include "timespec.h"
#include "unused.h"
#include "misc.h"

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

#define FRIEND_PURGE_INTERVAL SEC_PER_HOUR * HOUR_PER_DAY * 30
#define GROUP_PURGE_INTERVAL 3600

void self_connection_status(Tox *UNUSED(tox), TOX_CONNECTION status, void *UNUSED(userData))
{
    if (status == TOX_CONNECTION_NONE) {
        yinfo("Lost connection to the tox network");
    } else {
        yinfo("Connected to the tox network, status: %d", status);
    }
}

/* TODO: hardcoding is bad stop being lazy */
static struct toxNodes {
    const char *ip;
    uint16_t    port;
    const char *key;
} nodes[] = {
    { "144.76.60.215",   33445, "04119E835DF3E78BACF0F84235B300546AF8B936F035185E2A8E9E0A67C8924F" },
    { "192.210.149.121", 33445, "F404ABAA1C99A9D37D61AB54898F56793E1DEF8BD46B1038B9D822E8460FAB67" },
    { "195.154.119.113", 33445, "E398A69646B8CEACA9F0B84F553726C1C49270558C57DF5F3C368F05A7D71354" },
    { "46.38.239.179",   33445, "F5A1A38EFB6BD3C2C8AF8B10D85F0F89E931704D349F1D0720C3C4059AF2440A" },
    { "76.191.23.96",    33445, "93574A3FAB7D612FEA29FD8D67D3DD10DFD07A075A5D62E8AF3DD9F5D0932E11" },
    { NULL, 0, NULL },
};

static void bootstrap_DHT(Tox *m)
{
    int i;
    uint8_t key_bin[TOX_PUBLIC_KEY_SIZE];

    for (i = 0; nodes[i].ip; ++i) {
        sodium_hex2bin(key_bin, TOX_PUBLIC_KEY_SIZE, nodes[i].key, strlen(nodes[i].key), NULL, NULL, NULL);

        TOX_ERR_BOOTSTRAP err;
        tox_bootstrap(m, nodes[i].ip, nodes[i].port, key_bin, &err);

        if (err != TOX_ERR_BOOTSTRAP_OK)
            yerr("Failed to bootstrap DHT via: %s %d (error %d)", nodes[i].ip, nodes[i].port, err);
    }
}

static void handle_signal(int UNUSED(sig))
{
    signal_exit = true;
}

struct suit_info *suit_info_new(struct suit_info *si)
{
    si = malloc(sizeof(struct suit_info));
    si->tox = NULL;
    si->toxav = NULL;
    si->start_time = time(NULL);
    si->data_filename = strdup("data.tox");
    si->last_purge = 0;
    si->version = malloc(40);
    sprintf(si->version,"%s-%s", VERSION, GIT_VERSION);

    INIT_LIST_HEAD(&si->friends_info);
    return si;
}

void callbacks_init(struct suit_info *si)
{
    TOXAV_ERR_NEW toxav_err;

    tox_callback_self_connection_status(si->tox, self_connection_status, NULL);

    tox_callback_friend_message(si->tox, friend_message, si);
    tox_callback_friend_request(si->tox, friend_request_cb, &si->friends_info);
    tox_callback_friend_name(si->tox, friend_name_cb, &si->friends_info);
    tox_callback_friend_status(si->tox, friend_status_cb, &si->friends_info);
    tox_callback_friend_connection_status(si->tox, friend_connection_status_cb, &si->friends_info);
    tox_callback_friend_status_message(si->tox, friend_status_message_cb, &si->friends_info);

    tox_callback_file_recv_chunk(si->tox, write_file, NULL);
    tox_callback_file_recv_control(si->tox, file_print_control, NULL);
    tox_callback_file_recv(si->tox, file_request_accept, NULL);

    tox_callback_file_chunk_request(si->tox, tox_file_chunk_request, NULL);

    si->toxav = toxav_new(si->tox, &toxav_err);
    if (toxav_err != TOXAV_ERR_NEW_OK) {
        ywarn("Error at toxav_new: %d", toxav_err);
    }
}


int main(int argc, char **argv)
{
    ylog_set_level(YLOG_DEBUG, getenv("YLOG_LEVEL"));
    signal(SIGINT, handle_signal);

    size_t friend_list_size;

    /* defaults parameters */
    const char *passphrase = "somethingintheway" ;
    const char *name = "Suit";
    const char *status_msg = "A Tox service bot based on ToxSuite.";


    /* Load datas
     *
     */

    struct suit_info *si = NULL;
    si = suit_info_new(si);
    yinfo("ToxSuite v%s",si->version);

    TOX_ERR_NEW err = TOX_ERR_NEW_OK;
    struct Tox_Options options;
    tox_options_default(&options);


    //    load_json_profile(&si->tox, &options);

    if (file_exists(si->data_filename)) {
        if (!load_profile(&si->tox, &options,si->data_filename, passphrase)) {
            yinfo("Failed to load data from disk");
            return -1;
        }
    } else {
        yinfo("Creating a new profile");
        si->tox = tox_new(&options, &err);
        tox_self_set_name(si->tox, (uint8_t *)name, strlen(name), NULL);
        tox_self_set_status_message(si->tox, (uint8_t *)status_msg, strlen(status_msg), NULL);
        toxdata_set_shouldSaveConfig(true);
    }

    if (err != TOX_ERR_NEW_OK) {
        ywarn("Error at tox_new, error: %d", err);
        return -1;
    }

    /* Get connection.
     *
     */
    bootstrap_DHT(si->tox);

    /*
     * Initialisation of modules
     */
    client_info_new(&si->own_info);
    client_info_update(si->tox, &si->own_info);
    client_info_print(stdout, &si->own_info);
    callbacks_init(si);
    calltest_init(si->toxav);
    friends_info_init(si->tox, &si->friends_info, &friend_list_size);

    uint64_t tox_delay = 0,
            toxav_delay = 0,
            calltest_delay = 0,
            sleep_delay = 0;

    int i = 0;

    // initial to timespec_now(&tp1);?
    struct timespec tpnow;
    struct timespec tsprev = { 0, 0 };
    struct timespec tstox = { 0, 0 };
    struct timespec tsav = { 0, 0 };
    struct timespec tsfriend = { 0, 0 };
    struct timespec tscalltest = { 0, 0 };
//    struct timespec tssleep = { 0, 0 };
    struct timespec tselasped = { 0, 0 };
//    struct timespec tsantispam = { 0, 0 };

//    memcpy(&tsprev, &tscalltest, sizeof(struct timespec));

    /* polling loop
     * by order of priority (max priority at top list)
     */
    ydebug("entering busy-loop");
    while (!signal_exit) {

        i++;
        memcpy(&tsprev, &tpnow, sizeof(struct timespec));
        timespec_now(&tpnow);

        calltest_delay = calltest_iteration_interval();
        if (nanosec_timeout(&tscalltest, &tpnow, calltest_delay)) {
            calltest_iterate(si, &tpnow);
            memcpy(&tscalltest, &tpnow, sizeof(struct timespec));
            continue;
        }

        tox_delay = tox_iteration_interval(si->tox) * NSEC_PER_MSEC;
        if (nanosec_timeout(&tstox, &tpnow, tox_delay)) {
            tox_iterate(si->tox);
            memcpy(&tstox, &tpnow, sizeof(struct timespec));
            continue;
        }

        toxav_delay = toxav_iteration_interval(si->toxav) * NSEC_PER_MSEC;
        if (nanosec_timeout(&tsav, &tpnow, toxav_delay)) {
            toxav_iterate(si->toxav);
            memcpy(&tsav, &tpnow, sizeof(struct timespec));
            continue;
        }

        /* antispam */
//        if (nanosec_timeout(&tsantispam, &tpnow, 10 * NSEC_PER_SEC)) {
//            basic_antispam(si-tox, FRIEND_PURGE_INTERVAL, timespec_to_ns(&tpnow), &si->friends_info);
//            memcpy(&tsantispam, &tpnow, sizeof(struct timespec));
//            toxdata_set_shouldSaveConfig(true);
//            continue;
//        }

        if (nanosec_timeout(&tsfriend, &tpnow, FRIEND_PURGE_INTERVAL * NSEC_PER_SEC)) {
            friend_cleanup(si->tox, FRIEND_PURGE_INTERVAL, timespec_to_ns(&tpnow), &si->friends_info);
            memcpy(&tsfriend, &tpnow, sizeof(struct timespec));
            toxdata_set_shouldSaveConfig(true);
            continue;
        }

        /* encryption could take some time - very low priority
         * too much called, IMPROVE
         */
        if (toxdata_get_shouldSaveConfig())
        {
            ydebug("suit saves toxdata");
            toxdata_set_shouldSaveConfig(false);
            save_profile(si->tox,si->data_filename, passphrase);
        }

        sleep_delay = min(3, calltest_delay, tox_delay, toxav_delay);
        timespec_subtract(&tselasped, &tpnow, &tsprev); // elasped time
        ytrace("elasped time { %ju, %ju}",tselasped.tv_sec, tselasped.tv_nsec);
        sleep_delay -= timespec_to_ns(&tselasped);
        sleep_delay /= 10; // resolution = 1/10
        ytrace("elasped time { %ju, %ju}, sleep_delay = %ju",tselasped.tv_sec, tselasped.tv_nsec, sleep_delay);
        ytrace("sleep_delay = %ju",sleep_delay);
        nanosleep((const struct timespec[]){{0, sleep_delay }}, NULL);
    }

    ywarn("Killing tox and saving profile");
    save_profile(si->tox,si->data_filename, passphrase);
    calltest_destroy(si->toxav);
    toxav_kill(si->toxav);
    tox_kill(si->tox);
    return 0;
}
