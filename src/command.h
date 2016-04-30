#ifndef COMMAND_H
#define COMMAND_H

#include <time.h>
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

#include <tox/tox.h>
#include <tox/toxav.h>

int argexp(const uint8_t *message, size_t length, char ***local_argv, int *local_argc, const char startc);
void friend_message(Tox *tox, uint32_t friend_number, TOX_MESSAGE_TYPE type, const uint8_t *message, size_t length, void *user_data);

#endif
