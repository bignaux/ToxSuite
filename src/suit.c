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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include <sodium.h>

#include "fileop.h"
#include "filesend.h"
#include "callbacks.h"
#include "tsfiles.h"

static bool signal_exit = false;

#define FRIEND_PURGE_INTERVAL SEC_PER_HOUR * HOUR_PER_DAY * 30
#define GROUP_PURGE_INTERVAL 3600
#define TOXXD true

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
{ "127.0.0.1",       33445, "728925473812C7AAC482BE7250BCCAD0B8CB9F737BF3D42ABD34459C1768F854"},
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

    /*tox-xd hack*/
    /* Callbacks */
    if (TOXXD)
    {
        tox_callback_friend_message(si->tox, on_message, NULL);
        file_new_set_callback(on_new_file);
    } else {
        tox_callback_friend_message(si->tox, friend_message, si);
    }

    /* suit core*/

    tox_callback_friend_request(si->tox, friend_request_cb, &si->friends_info);
    tox_callback_friend_name(si->tox, friend_name_cb, &si->friends_info);
    tox_callback_friend_status(si->tox, friend_status_cb, &si->friends_info);
    tox_callback_friend_connection_status(si->tox, friend_connection_status_cb, &si->friends_info);
    tox_callback_friend_status_message(si->tox, friend_status_message_cb, &si->friends_info);

    tox_callback_file_chunk_request(si->tox, file_chunk_request_cb, NULL);
    tox_callback_file_recv_control(si->tox, file_recv_control_cb, NULL);
    tox_callback_file_recv_chunk(si->tox, file_recv_chunk_cb, NULL);

    tox_callback_file_recv(si->tox, file_recv_cb, NULL);

    si->toxav = toxav_new(si->tox, &toxav_err);
    if (toxav_err != TOXAV_ERR_NEW_OK) {
        ywarn("Error at toxav_new: %d", toxav_err);
    }
}

void redirect_output()
{
    // these lines are to direct the stdout and stderr to log files we can access even when run as a daemon (after the possible help info is displayed.)
    //open up the files we want to use for out logs
    uint32_t new_stderr, new_stdout;
    new_stderr = open("suit_error.log", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    new_stdout = open("suit_debug.log", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    //truncate those files to clear the old data from long since past.
    if (0 != ftruncate(new_stderr, 0))
    {
        perror("could not truncate stderr log");
    }
    if (0 != ftruncate(new_stdout, 0))
    {
        perror("could not truncate stdout log");
    }

    //duplicate the new file descriptors and assign the file descriptors 1 and 2 (stdout and stderr) to the duplicates
    dup2(new_stderr, STDERR_FILENO);
    dup2(new_stdout, STDOUT_FILENO);

    //now that they are duplicated we can close them and let the overhead c stuff worry about closing stdout and stderr later.
    close(new_stderr);
    close(new_stdout);
    close(STDIN_FILENO);
}

int main(int argc, char **argv)
{
    const char *passphrase = getenv("SUIT_PASSPHRASE");
    const char *name = getenv("SUIT_NAME");
    const char *status_msg = getenv("SUIT_STATUSMSG");
    const char *home = getenv("SUIT_HOME");
    const char *cachedir_path = "/tmp/testcache";
    const char *shareddir_path = "/usr/share/zoneinfo/Europe/";

    signal(SIGINT, handle_signal);

    size_t size;

    /* Our process ID and Session ID */
    pid_t pid, sid;

    /* Fork off the parent process */
//    pid = fork();
//    if (pid < 0) {
//        exit(EXIT_FAILURE);
//    }
//    /* If we got a good PID, then
//       we can exit the parent process. */
//    if (pid > 0) {
//        exit(EXIT_SUCCESS);
//    }

//    /* Change the file mode mask */
//    umask(0);

//    /* Open any logs here */
    ylog_set_level(YLOG_DEBUG, getenv("YLOG_LEVEL"));

//    /* Create a new SID for the child process */
//    sid = setsid();
//    if (sid < 0) {
//        /* Log the failure */
//        exit(EXIT_FAILURE);
//    }

    /* Change the current working directory */
    int fdir = open(home, O_DIRECTORY);
    if (fdir == -1)  {
        yerr("Suit can't change home to %s : %s", home, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if ((fchdir(fdir)) < 0) {
        /* Log the failure */
        exit(EXIT_FAILURE);
    }
    close(fdir);

    //    redirect_output();

    /* Load datas
     *
     */

    struct suit_info *si = NULL;
    si = suit_info_new(si);
    yinfo("ToxSuite version %s",si->version);

    TOX_ERR_NEW err = TOX_ERR_NEW_OK;
    struct Tox_Options options;
    tox_options_default(&options);
    print_option(&options);

    //    load_json_profile(&si->tox, &options);

    if (!access(si->data_filename, R_OK|W_OK)) {
        if (load_profile(&si->tox, &options,si->data_filename, passphrase)) {
            yinfo("Failed to load data from disk");
            return EXIT_FAILURE;
        }
    } else {
        yinfo("Creating a new profile");
        si->tox = tox_new(&options, &err);
        toxdata_set_shouldSaveConfig(true);
        if (err != TOX_ERR_NEW_OK) {
            ywarn("Error at tox_new, error: %d", err);
            return EXIT_FAILURE;
        }
    }

    /* set to user config
     *
     */
    if(name) {
        size = strlen(name);
        if( size > TOX_MAX_NAME_LENGTH)
        {
            size = TOX_MAX_NAME_LENGTH;
            ywarn("$SUIT_NAME is too long,will be truncated.");
        }
        tox_self_set_name(si->tox, (uint8_t *)name, size, NULL);
    }
    if(status_msg) {
        size = strlen(status_msg);
        if (size > TOX_MAX_STATUS_MESSAGE_LENGTH)
        {
            size = TOX_MAX_STATUS_MESSAGE_LENGTH;
            ywarn("$SUIT_STATUSMSG is too long,will be truncated");
        }
        tox_self_set_status_message(si->tox, (uint8_t *)status_msg, size, NULL);
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
    friends_info_init(si->tox, &si->friends_info, &size);

    uint64_t tox_delay = 0,
            toxav_delay = 0,
            calltest_delay = 0,
            sleep_delay = 0;

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



    int rc = filenode_load_fromdir(cachedir_path);
    yinfo("Loaded %i files from cache", rc);
    /* checks if loaded files actually exist */
    file_recheck_callback(SIGUSR1);
    FileQueue_init(&FilesSender);
    FileQueue_init(&FileQueueLoaded);
    load_senders(&FileQueueLoaded, &si->friends_info);

    /* polling loop
     * by order of priority (max priority at top list)
     */
    ydebug("entering busy-loop");
    while (!signal_exit) {

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

        // see file_recheck
        if (nanosec_timeout(&tsfriend, &tpnow, 5000 * NSEC_PER_USEC)) {
            file_do(shareddir_path, cachedir_path);
        }

        /* encryption could take some time - very low priority
         * too much called, IMPROVE
         */
        toxdata_set_shouldSaveConfig(false);
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
        memcpy(&tsprev, &tpnow, sizeof(struct timespec));
    }

    ywarn("SIGINT/SIGTERM received, terminating...");
    save_senders(&FilesSender, &si->friends_info);
    save_profile(si->tox,si->data_filename, passphrase);
    calltest_destroy(si->toxav);
    FileQueue_destroy(&FilesSender);
    FileQueue_destroy(&FileQueueLoaded);
    toxav_kill(si->toxav);
    tox_kill(si->tox);
    return EXIT_SUCCESS;
}
