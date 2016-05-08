#ifndef MISC_H
#define MISC_H

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

#include <tox/tox.h>
#include <tox/toxav.h>
#include <sodium.h>
#include "ylog/ylog.h"

struct client_info {
    uint8_t *name;
    uint8_t *status_message;
    uint8_t tox_id_bin[TOX_ADDRESS_SIZE];
    char tox_id_hex[TOX_ADDRESS_SIZE * 2 + 1];
    TOX_CONNECTION connect;
};

struct toxstream_cookie {
    Tox *tox;
    uint32_t friend_number;
    TOX_MESSAGE_TYPE type;
};

//ugly capcha library
#ifdef CAPTCHA
const int gifsize;
void captcha(unsigned char im[70*200], unsigned char l[6]);
void makegif(unsigned char im[70*200], unsigned char gif[gifsize]);
void generate_capcha(unsigned char gif[gifsize], unsigned char l[6]);
#endif

//#define perrlog(msg) yerr("%s: %s", msg, strerror(errno))
#define perrlog(msg) yerr("%s", msg)

void human_readable_filesize(char *dest, off_t size);
char *human_readable_id(uint8_t *address, uint16_t length);
char *gnu_basename(char *path);

ssize_t toxstream_write(void *c, const char *message, size_t size);
void print_secretkey(FILE *stream, const Tox *tox);
void client_info_destroy(struct client_info *own_info);
void client_info_new(struct client_info *own_info);
void client_info_print(FILE *stream, const struct client_info *own_info);
void client_info_update(const Tox *apptox, struct client_info *own_info);

void get_elapsed_time_str(char *buf, int bufsize, uint64_t secs);
bool load_json_profile(Tox **tox, struct Tox_Options *options);
int min(int arg_count, ...);
void print_option(struct Tox_Options *options);

#endif
