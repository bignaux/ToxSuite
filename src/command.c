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

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include "suit.h"
#include "friend.h"
#include "misc.h"
#include "file.h"
#include "ylog/ylog.h"
#include "unused.h"

#include <getopt.h>

void send_capcha(Tox *tox, uint32_t friend_number,const unsigned char gif[gifsize], const unsigned char l[6])
{
    FILE *capchafile = NULL;
    char capchafilename[] = "/tmp/capcha-XXXXXX.gif";
    int fd = mkstemps(capchafilename, 4);
    close(fd);
    capchafile = fopen(capchafilename, "wb");
    fwrite(gif,gifsize,1,capchafile);
    fclose(capchafile);
    add_filesender(tox, friend_number, capchafilename);
}

/**
 * @brief format friends mesg to be parse by getopt_long()
 * @param command
 * @param length
 * @param local_argv for getopt_long()
 * @param local_argc for getopt_long()
 * @param delim : start command delimitor
 * @return 0 if success , -1 if failure.
 */
int argexp(const uint8_t *command, size_t length, char ***local_argv, int *local_argc, const char delim)
{
	//char **local_argv = NULL;
	const char* prefix = "arg0 "; // getopt_long expects that the actual arguments starts at argv[1]
	size_t line_len = strlen(prefix) + length + 2;
	char* line_str = NULL;
	int cnt = 0;

    if (command[0] != delim) {
		return -1;
	}

	line_str = malloc(line_len);
    snprintf(line_str, line_len, "%s--%s", prefix, command + 1);
	char* p = strtok(line_str, " ");
	/* split string and append tokens to 'argv' */

	// see how many strings we need
	while (p) {
		cnt++;                                                                          // count strings
		*local_argv = realloc(*local_argv, sizeof(char*) * cnt);                        // resize array of strings
		(*local_argv)[cnt - 1] = malloc(strlen(p) + 1);                                 // allocate memory for string
		(*local_argv)[cnt - 1] = memcpy((*local_argv)[cnt - 1], p, strlen(p) + 1);      // copy string to array
		p = strtok(NULL, " ");
	}

	// realloc one extra element for the last NULL */

	// @genesis not sure the stuff below is really needed
	//    *local_argv = realloc (*local_argv, sizeof (char*) * (cnt+1));
	//    (*local_argv)[cnt] = 0;

	*local_argc = cnt;

	//    for (int i = 0; i < (cnt); ++i)
	//        printf ("argv[%d] = %s\n", i, (*local_argv)[i]);

	free(line_str);
	return 0;
}


void friend_message(Tox *tox, uint32_t friend_number, TOX_MESSAGE_TYPE __attribute__((unused))type, const uint8_t *message, size_t length, void *user_data)
{
    cookie_io_functions_t toxstream_func = {
        .read	= NULL,
        .write	= toxstream_write,
        .seek	= NULL,
        .close	= NULL
    };

	FILE *stream;
    struct toxstream_cookie mycookie = {
		.tox		= tox,
		.friend_number	= friend_number,
        .type		= TOX_MESSAGE_TYPE_NORMAL,
	};

	stream = fopencookie(&mycookie, "w", toxstream_func);
	if (stream == NULL) {
		perror("fopencookie");
		return;
	}

    ydebug("Recv: %s", message);

    struct suit_info *si = user_data;
    struct list_head *friends_info = &si->friends_info;
    struct friend_info *f = friend_info_by_friend_number(friends_info, friend_number);

    int digit_optind = 0;
	int argc;
	char **argv = NULL;
    int ret = argexp(message, length, &argv, &argc, '!');
    if (ret)
        return;

	optind = 0; // /!\ magic here ...

	for (int i = 0; i < argc; i++) {
        yinfo("ARGV[%d]=%s", i, argv[i]);
	}
    yinfo("ARGC=%d", argc);
	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			{ "addfriend",	required_argument,     NULL,		     'a' },
			{ "banner",	no_argument,	       NULL,		     'b' },
			{ "callme",	optional_argument,     NULL,		     'c' },
            { "capcha", no_argument,	       NULL,		     'd' },
			{ "forgetme",	no_argument,	       NULL,		     'g' },
			{ "friends",	no_argument,	       NULL,		     'f' },
			{ "info",	no_argument,	       NULL,		     'i' },
            { "sendimage", no_argument,	       NULL,		     't' },
			{ "pamarkdown", no_argument,	       NULL,		     'p' },
            { "shema", no_argument,	       NULL,		     's' },
            { "whoiam", no_argument,	       NULL,		     'w' },
			{ NULL,		0,		       NULL,		     0	 }
		};

		int c = getopt_long(argc, argv, "a:bcgfi",
				    long_options, &option_index);

        yinfo("c=%d", c);

		if (c == -1)
			break;

		switch (c) {
		case 0:
			fprintf(stream, "option %s", long_options[option_index].name);
			if (optarg)
				fprintf(stream, " with arg %s", optarg);
			fprintf(stream, "\n");
			break;

		case '0':
		case '1':
		case '2':
			if (digit_optind != 0 && digit_optind != this_option_optind)
				fprintf(stream, "digits occur in two different argv-elements.\n");
			digit_optind = this_option_optind;
			fprintf(stream, "option %c\n", c);
			break;
		case 'a':
			/* !addfriend
			 *
			 */
			break;
        case 'i':
            fprintf(stream,"ToxSuite v%s\n",si->version);
            client_info_print(stream, &si->own_info);
			break;

		case 'b':
			/* !banner
			 * https://github.com/tux3/qTox/pull/3008
			 */
			fprintf(stream, "`      :::::::::: ::::::::  :::    :::  ::::::::  :::::::::   :::::::: :::::::::::`\n");
			fprintf(stream, "`     :+:       :+:    :+: :+:    :+: :+:    :+: :+:    :+: :+:    :+:    :+:`\n");
			fprintf(stream, "`    +:+       +:+        +:+    +:+ +:+    +:+ +:+    +:+ +:+    +:+    +:+`\n");
			fprintf(stream, "`   +#++:++#  +#+        +#++:++#++ +#+    +:+ +#++:++#+  +#+    +:+    +#+`\n");
			fprintf(stream, "`  +#+       +#+        +#+    +#+ +#+    +#+ +#+    +#+ +#+    +#+    +#+`\n");
			fprintf(stream, "` #+#       #+#    #+# #+#    #+# #+#    #+# #+#    #+# #+#    #+#    #+#`\n");
			fprintf(stream, "`########## ########  ###    ###  ########  #########   ########     ###`\n");
			break;

		case 'c':
			/* !callme
			 *
			 */
            toxav_call(si->toxav, friend_number, AUDIO_BITRATE, 0, NULL);
			break;

        case 'f':
            friends_info_print(stream, friends_info);
			break;

        case 'd':;
            unsigned char response[6];
            unsigned char gif[17646];

            generate_capcha(gif, response);
            send_capcha(tox, friend_number, gif, response);
            fprintf(stream, "response : %s\n", response);
            break;

		case 'g':
			fprintf(stream, "Bye old friend... I'll never forget you!\n");
			tox_friend_delete(tox, friend_number, NULL);
			break;

		case 'p':;
			/* very long file to test fopencookie */
			FILE * file = fopen("pa.md", "r");
            int unused __attribute__((unused));

			fseek(file, 0L, SEEK_END);
			long file_size = ftell(file);
			fseek(file, 0L, SEEK_SET);
            char *buffer = malloc(file_size + 1);


            unused = fread(buffer, sizeof(char), file_size, file);
			fclose(file);
            buffer[file_size] = '\0';
			fprintf(stream, buffer);
			break;
        case 's':
            fprintf(stream,
                    "                 acti^e.keys                         expired.keys\n"
                    "              +--------------+                    +--------------+\n"
                    "     add()    |              |    trash()         |              |  purge()\n"
                    "+------------^+              +-------------------^+              +---------->\n"
                    "              |              |                    |              |\n"
                    "              |              |                    |              |\n"
                    "              |              |                    |              |\n"
                    "              |  publickeys  |                    |  publickeys  |\n"
                    "              |              |                    |              |\n"
                    "              |              |    untrash()       |              |\n"
                    "              |              +^-------------------+              |\n"
                    "              |              |                    |              |\n"
                    "              |              |                    |              |\n"
                    "              +--------------+                    +--------------+\n");
            break;
        case 't':;
            char * outfilename = "85B648131DD80FC1DD5F453DE3AC8584B7EB34CC0AD3E7AE5E2245A08161386F.png";
            add_filesender(si->tox, friend_number, outfilename);
            break;

        case 'w':;
            friend_info_print(stream, f);
            break;

		case '?':
			fprintf(stream, "option %s not implemented,optopt=%c\n", argv[1], optopt);
			break;


		default:
			fprintf(stream, "?? getopt returned character code 0%o ??\n", c);
		}
	}

	if (optind < argc) {
		fprintf(stream, "non-option ARGV-elements: ");
		while (optind < argc)
			fprintf(stream, "%s ", argv[optind++]);
		fprintf(stream, "\n");
	}
	fclose(stream);
	for (int i = 0; i < argc; i++)
		free(argv[i]);
}