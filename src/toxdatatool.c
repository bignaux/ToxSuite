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

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <popt.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "friend.h"
#include "toxdata.h"
#include "misc.h"

int main(int argc, char *argv[])
{
    struct Tox_Options options;
    Tox *apptox = NULL;
    struct client_info own_info;
    size_t friend_list_size;

    struct list_head friends_info;
    INIT_LIST_HEAD(&friends_info);

    //workaround for old glibc
//    struct timespec ts;
//    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

    bool saveflag = false;
    char *passphrase = NULL;
    char *toxfile = NULL;
    char *exportfile = NULL;
    char *importfile = NULL;
    char *removefile = NULL;
    uint64_t current_time;

    int verbose = 0, print = 0;
    int retcode = EXIT_SUCCESS;

    uint64_t purge_delay = 0;

    poptContext pc;
    struct poptOption po[] = {
    { "passphrase",'s', POPT_ARG_STRING,   &passphrase, 11002,	    "Use passphrase to encrypt/decrypt toxfile.",      "<passphrase>"				       },
    { "export",  'x',   POPT_ARG_STRING,   &exportfile, 11002,	    "Export friends's tox publickey into file.",      "<tox.keys>"				       },
    { "import",  'i',   POPT_ARG_STRING,   &importfile, 11003,	    "Import friends from tox publickey file.",	      "<tox.keys>"				       },
    { "remove",  'r',   POPT_ARG_STRING,   &removefile, 11004,	    "Remove friends from tox publickey file.",	      "<expire.keys>"				       },
    { "purge",   'u',   POPT_ARG_LONGLONG, &purge_delay, 11007,	    "Purge friends toxfile not seen since <delay>", "<delay in second>"					       },
    { "print",   'p',   POPT_ARG_NONE,     &print,	    11005,	    "Print content of toxfile (after modification if any).",		      ""					       },
    { "verbose", 'v',   POPT_ARG_NONE,     &verbose,    11006,	    "Be verbose",				      ""					       },
    POPT_AUTOHELP
    POPT_TABLEEND
};

    // pc is the context for all popt-related functions
    pc = poptGetContext(NULL, argc, (const char**)argv, po, 0);
    poptSetOtherOptionHelp(pc, "<toxfile>");
    if (argc < 2) {
        poptPrintUsage(pc, stderr, 0);
        exit(1);
    }

    // process options and handle each val returned
    int val;
    while ((val = poptGetNextOpt(pc)) >= 0) {
        //        printf("poptGetNextOpt returned val %d\n", val);
    }

    // poptGetNextOpt returns -1 when the final argument has been parsed
    // otherwise an error occured
    if (val != -1) {
        // handle error
        switch (val) {
        case POPT_ERROR_NOARG:
            printf("Argument missing for an option\n");
            exit(EXIT_FAILURE);
        case POPT_ERROR_BADOPT:
            printf("Option's argument could not be parsed\n");
            exit(EXIT_FAILURE);
        case POPT_ERROR_BADNUMBER:
        case POPT_ERROR_OVERFLOW:
            printf("Option could not be converted to number\n");
            exit(EXIT_FAILURE);
        default:
            printf("Unknown error in option processing\n");
            exit(EXIT_FAILURE);
        }
    }

    // Handle ARG... part of commandline
    while (poptPeekArg(pc) != NULL) {
        char *arg = (char*)poptGetArg(pc);
        //        printf("poptGetArg returned arg: \"%s\"\n", arg);
        toxfile = strdup(arg);
    }

    poptFreeContext(pc);

    if (!toxfile) {
        printf("You should provides a tox file.");
        retcode = EXIT_FAILURE;
        goto cleanup;
    }

    if (passphrase) {
        saveflag = true; // TODO improve this
    }

    tox_options_default(&options);

    if (file_exists(toxfile)) {
        if (load_profile(&apptox, &options, toxfile, passphrase)) {
            //            print_option(&options);
        } else {
            printf("Failed to load data from disk\n");
            return EXIT_FAILURE;
        }
    }

    client_info_new(&own_info);
    client_info_update(apptox, &own_info);
    friends_info_init(apptox, &friends_info, &friend_list_size);

    if (exportfile)
        export_keys(exportfile, &friends_info);

    if (importfile) {
        import_keys(apptox, &friends_info, importfile);
        saveflag = true;
    }

    if (removefile) {
        remove_keys(apptox, &friends_info, removefile);
        saveflag = true;
    }

    if (purge_delay > 0) {
        current_time = time(NULL);
        friend_cleanup(apptox, purge_delay, current_time, &friends_info);
        saveflag = true;
    }

    if (saveflag) {
        save_profile(apptox, toxfile, passphrase);
    }

    if (print) {
//        print_secretkey(stdout, tox);
        client_info_print(stdout, &own_info);
        friends_info_print(stdout, &friends_info);
    }

cleanup:

    if (own_info.name) client_info_destroy(&own_info);
    if (!list_empty(&friends_info)) friends_info_destroy(&friends_info);
    if (apptox) tox_kill(apptox);
    exit(retcode);
}
