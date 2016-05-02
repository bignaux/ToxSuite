#ifndef FILE_H
#define FILE_H

#define STRING_LENGTH 256
#define HISTORY 50

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

int printf_file(FILE* stream, const char *filename);

void file_request_accept(Tox *tox, uint32_t friend_number, uint32_t file_number, uint32_t type, uint64_t file_size,
			 const uint8_t *filename, size_t filename_length, void *user_data);
void file_print_control(Tox *tox, uint32_t friend_number, uint32_t file_number, TOX_FILE_CONTROL control,
			void *user_data);
void write_file(Tox *tox, uint32_t friendnumber, uint32_t filenumber, uint64_t position, const uint8_t *data,
		size_t length, void *user_data);

/*******************************************************************************
 *
 * :: File transmission: sending
 *
 ******************************************************************************/

uint32_t add_filesender(Tox *m, uint32_t friendnum, char *filename);

void tox_file_chunk_request(Tox *tox, uint32_t friend_number, uint32_t file_number, uint64_t position, size_t length,
                void *user_data);

#endif
