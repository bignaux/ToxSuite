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

/*
 * some func are from https://github.com/irungentoo/toxcore/blob/master/testing/nTox.c
 *
 */

#include "file.h"
#include "unused.h"

#define NUM_FILE_SENDERS 64
typedef struct {
	FILE *file;
	uint32_t friendnum;
	uint32_t filenumber;
} File_Sender;
File_Sender file_senders[NUM_FILE_SENDERS];
uint8_t numfilesenders;

int printf_file(FILE* stream, const char *filename)
{
    FILE * file;
    size_t file_size;
    char *buffer;
    size_t szread;
    int retcode = 0;

    if(!access(filename, R_OK)) {
        ydebug("Try to open %s : no read access.",filename);
        return -1;
    }
    file = fopen(filename, "r");
    if (!file) {
        ydebug("Try to open %s : fopen error.",filename);
        return -1;
    }

    fseek(file, 0L, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0L, SEEK_SET);

    buffer = malloc(file_size + 1);
    szread = fread(buffer, sizeof(char), file_size, file);
    fclose(file);
    if (szread == file_size)
    {
        buffer[file_size] = '\0';
        fprintf(stream, "%s", buffer);
    } else retcode = -1;
    free(buffer);
    return retcode;
}

void tox_file_chunk_request(Tox *tox, uint32_t friend_number, uint32_t file_number, uint64_t position, size_t length,
			    void *user_data)
{
	unsigned int i;
    uint8_t *data;

	for (i = 0; i < NUM_FILE_SENDERS; ++i) {
		/* This is slow */
		if (file_senders[i].file && file_senders[i].friendnum == friend_number && file_senders[i].filenumber == file_number) {
			if (length == 0) {
				fclose(file_senders[i].file);
				file_senders[i].file = 0;
                yinfo("%u file transfer: %u completed", file_senders[i].friendnum, file_senders[i].filenumber);
				break;
			}

			fseek(file_senders[i].file, position, SEEK_SET);
            data = malloc(length);
			int len = fread(data, 1, length, file_senders[i].file);
			tox_file_send_chunk(tox, friend_number, file_number, position, data, len, 0);
            free(data);
			break;
		}
	}
}

// TODO : we could known size of our own file
// TODO : bool erase_after_send
// TODO : add explicit filename "capcha"... without path (basename) or mktemp
uint32_t add_filesender(Tox *m, uint32_t friendnum, char *filename)
{
	FILE *tempfile = fopen(filename, "rb");
    int64_t filesize;

    if (tempfile == NULL)
		return -1;

	fseek(tempfile, 0, SEEK_END);
    filesize = ftell(tempfile);
	fseek(tempfile, 0, SEEK_SET);

	uint32_t filenum = tox_file_send(m, friendnum, TOX_FILE_KIND_DATA, filesize, 0, (uint8_t*)filename,
					 strlen(filename), 0);

    if (filenum == UINT32_MAX)
        return -1;

	file_senders[numfilesenders].file = tempfile;
	file_senders[numfilesenders].friendnum = friendnum;
	file_senders[numfilesenders].filenumber = filenum;
	++numfilesenders;
	return filenum;
}

void file_request_accept(Tox *tox, uint32_t friend_number, uint32_t file_number, uint32_t type, uint64_t file_size,
			 const uint8_t *filename, size_t filename_length, void *user_data)
{
    if (type == TOX_FILE_KIND_AVATAR)
        yinfo("Avatar not supported yet.");
	if (type != TOX_FILE_KIND_DATA) {
        yinfo("Refused invalid file type.");
		tox_file_control(tox, friend_number, file_number, TOX_FILE_CONTROL_CANCEL, 0);
		return;
	}

	char msg[512];
    sprintf(msg, "%u is sending us: %s of size %llu", friend_number, filename, (long long unsigned int)file_size);
    yinfo("%s", msg);

	if (tox_file_control(tox, friend_number, file_number, TOX_FILE_CONTROL_RESUME, 0)) {
		sprintf(msg, "Accepted file transfer. (saving file as: %u.%u.bin)", friend_number, file_number);
        yinfo("%s", msg);
	} else
        yinfo("Could not accept file transfer.");
}

void file_print_control(Tox *tox, uint32_t friend_number, uint32_t file_number, TOX_FILE_CONTROL control,
			void *user_data)
{
	char msg[512] = { 0 };

    sprintf(msg, "control %u received", control);
    yinfo("%s", msg);

	if (control == TOX_FILE_CONTROL_CANCEL) {
		unsigned int i;

		for (i = 0; i < NUM_FILE_SENDERS; ++i) {
			/* This is slow */
			if (file_senders[i].file && file_senders[i].friendnum == friend_number && file_senders[i].filenumber == file_number) {
				fclose(file_senders[i].file);
				file_senders[i].file = 0;
				char msg[512];
                sprintf(msg, "%u file transfer: %u cancelled", file_senders[i].friendnum, file_senders[i].filenumber);
                yinfo("%s", msg);
                // TODO int unlink (const char *filename)
			}
		}
	}
}

void write_file(Tox *tox, uint32_t friendnumber, uint32_t filenumber, uint64_t position, const uint8_t *data,
		size_t length, void *user_data)
{
	if (length == 0) {
		char msg[512];
        sprintf(msg, "%u file transfer: %u completed", friendnumber, filenumber);
        yinfo("%s", msg);
		return;
	}

	char filename[256];
	sprintf(filename, "%u.%u.bin", friendnumber, filenumber);
	FILE *pFile = fopen(filename, "r+b");

	if (pFile == NULL)
		pFile = fopen(filename, "wb");

	fseek(pFile, position, SEEK_SET);

	if (fwrite(data, length, 1, pFile) != 1)
        yinfo("Error writing to file");

	fclose(pFile);
}
