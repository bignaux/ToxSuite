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
 * read http://www.cgsecurity.org/Articles/SecProg/Art5/
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "file.h"
#include "unused.h"

// Improve : would be FileSendQueue something
void FileQueue_init(struct list_head* FileQueue)
{
    INIT_LIST_HEAD(FileQueue);
}

void FileQueue_destroy(struct list_head* FileQueue)
{
    struct FileSender *f, *n;

    list_for_each_entry_safe(f, n, FileQueue, list)
    {
        FileSender_destroy(f);
    }
}

int FileQueue_size(struct list_head* FileQueue)
{
    struct FileSender* f;
    int i = 0;
    list_for_each_entry(f, FileQueue, list)
    {
        i++;
    }
    return i;
}

struct FileSender* FileSender_get(struct list_head* FileQueue, const uint32_t friend_number, const uint32_t file_number)
{
    struct FileSender* f;

    list_for_each_entry(f, FileQueue, list)
    {
        if ((f->friend_number == friend_number) && (f->file_number == file_number)) {
            return f;
        }
    }
    return NULL;
}

struct FileSender* FileSender_new(struct list_head* FileQueue)
{
    struct FileSender* f;
    f = malloc(sizeof(struct FileSender));
    memset(f, 0, sizeof(struct FileSender));
    f->info = malloc(sizeof(struct FileNode));
    memset(f->info, 0, sizeof(struct FileNode));
    /* set default values */
    //    f->kind = TOX_FILE_KIND_DATA;
    //    f->file = NULL;
    list_add(&f->list, FileQueue);
    return f;
}

void FileSender_destroy(struct FileSender* f)
{
    list_del(&f->list);
    if(f->file) fclose(f->file);
    if(f->info) {
        ytrace("FileSender_destroy f->info");
        if(f->info->BLAKE2b) {
            ytrace("FileSender_destroy f->info->BLAKE2b");
            free(f->info->BLAKE2b);
        }
        if(f->info->file) {
            ytrace("FileSender_destroy f->info->file : %s", f->info->file);
            free(f->info->file);
        }
        ytrace("FileSender_destroy f->info");
        free(f->info);
    }
    ytrace("FileSender_destroy f");
    free(f);
}

/*******************************************************************************
 *
 * :: File transmission: sending
 *
 ******************************************************************************/

uint32_t add_filesender(Tox* tox, FileSender* f)
{
    TOX_ERR_FILE_SEND err;
    FileNode *fn = f->info;

    if (!f->file) {
        if (f->pathname) {
            f->file = fopen(f->pathname, "rb");
            if (!f->file) {
                yerr("add_filesender: couldn't open %s : %s", f->pathname, strerror(errno));
                return UINT32_MAX;
            }
        }
        else
            return UINT32_MAX;
    }

    // TODO : don't do this on stream
    if (!fn->length) {
        fseek(f->file, 0, SEEK_END);
        fn->length = ftell(f->file);
        fseek(f->file, 0, SEEK_SET);
    }

    ydebug("add_filesender %s", fn->file);

    f->file_number = tox_file_send(tox, f->friend_number, f->kind, fn->length, fn->BLAKE2b, (uint8_t*)fn->file,
                                   strlen(fn->file), &err);

    //TODO implement hash : file_checksumcalc_noblock
    //need an hash to resume file
    if(!fn->BLAKE2b) {
        fn->BLAKE2b = malloc(TOX_FILE_ID_LENGTH);
        //        memset(fn->BLAKE2b, 0, TOX_FILE_ID_LENGTH);
        tox_file_get_file_id(tox,  f->friend_number,  f->file_number, fn->BLAKE2b, NULL);
        ydebug("add_filesender need hash");
    }


    if (err != TOX_ERR_FILE_SEND_OK)
        ywarn("add_filesender error %d", err);

    return f->file_number; // UINT32_MAX => error
}


/*
 * Callback
 *
 */

void file_chunk_request_cb(Tox* tox, uint32_t friend_number, uint32_t file_number, uint64_t position, size_t length,
                           void* user_data)
{
    uint8_t* data;
    size_t len;
    FileSender* f = FileSender_get(&FilesSender, friend_number, file_number);

    if (!f || !f->file) {
        yerr("file_chunk_request_cb error : no open file descriptor.");
        return;
    }

    if (length == 0) {
        yinfo("%u file transfer: %u completed", f->friend_number, f->file_number);
        FileSender_destroy(f);
        return;
    }

    /* notice : since libsodium ensure validity of data, fseek() is only necessary for resume */
    // TODO : implement chunk-back storage
    if (fseek(f->file, position, SEEK_SET)) {
        ydebug("file_chunk_request_cb : fseek");
        return;
    }

    data = malloc(length);
    len = fread(data, 1, length, f->file);
    tox_file_send_chunk(tox, friend_number, file_number, position, data, len, 0);
    free(data);
}

void file_recv_control_cb(Tox* tox, uint32_t friend_number, uint32_t file_number, TOX_FILE_CONTROL control,
                          void* user_data)
{
    struct FileSender* f = FileSender_get(&FilesSender, friend_number, file_number);
    switch (control) {
    case TOX_FILE_CONTROL_PAUSE:
        yinfo("File transfer %d paused by friend %d", file_number, friend_number);
        //        f->accepted = false;
        break;
    case TOX_FILE_CONTROL_RESUME:
        yinfo("File transfer %d resumed by friend %d", file_number, friend_number);
        //        f->accepted = true;
        break;
    case TOX_FILE_CONTROL_CANCEL:
        yinfo("File transfer %d canceled by friend %d", file_number, friend_number);
        FileSender_destroy(f);
    }
}

/*******************************************************************************
 *
 * :: File transmission: receiving
 *
 ******************************************************************************/

void file_recv_cb(Tox* tox, uint32_t friend_number, uint32_t file_number, uint32_t type, uint64_t file_size,
                  const uint8_t* filename, size_t filename_length, void* user_data)
{
    if (type == TOX_FILE_KIND_AVATAR)
        yinfo("Avatar not supported yet.");
    if (type != TOX_FILE_KIND_DATA) {
        yinfo("Refused invalid file type.");
        tox_file_control(tox, friend_number, file_number, TOX_FILE_CONTROL_CANCEL, 0);
        return;
    }

    yinfo("%u is sending us: %s of size %llu", friend_number, filename, (long long unsigned int)file_size);

    if (tox_file_control(tox, friend_number, file_number, TOX_FILE_CONTROL_RESUME, 0)) {
        yinfo("Accepted file transfer. (saving file as: %u.%u.bin)", friend_number, file_number);
    }
    else
        yinfo("Could not accept file transfer.");
}

/* very poor design
 * look toxcore/testing/tox_sync.c => add file queue for user.
 */
void file_recv_chunk_cb(Tox* tox, uint32_t friend_number, uint32_t file_number, uint64_t position, const uint8_t* data,
                        size_t length, void* user_data)
{
    char filename[256];

    if (length == 0) {
        yinfo("%u file transfer: %u completed", friend_number, file_number);
        return;
    }

    sprintf(filename, "%u.%u.bin", friend_number, file_number);
    FILE* pFile = fopen(filename, "r+b");

    if (!pFile)
        pFile = fopen(filename, "wb");

    if (!pFile)
        return;

    if (!fseek(pFile, position, SEEK_SET)) {
        yerr("file_recv_chunk_cb : couldn't seek to position %ju", position);
        fclose(pFile);
        return;
    }

    if (fwrite(data, length, 1, pFile) != 1)
        yinfo("Error writing to file");

    fclose(pFile);
}

/*******************************************************************************
 *
 * :: Misc
 *
 ******************************************************************************/

int printf_file(FILE* stream, const char* filename)
{
    FILE* fp;
    size_t file_size;
    char* buffer;
    size_t szread;
    int retcode = EXIT_SUCCESS;

    struct stat st;
    int fd;

    if ((fd = open(filename, O_WRONLY, 0)) < 0) {
        yerr("Can't open %s\n", filename);
        exit(EXIT_FAILURE);
    }
    if (fstat(fd, &st)) {
        yerr("fstat error on %s!", filename);
        exit(EXIT_FAILURE);
    }
    if (st.st_uid != getuid()) {
        yerr("%s not owner !", filename);
        exit(EXIT_FAILURE);
    }
    if (!S_ISREG(st.st_mode)) {
        yerr("%s not a normal file", filename);
        exit(EXIT_FAILURE);
    }
    if ((fp = fdopen(fd, "w")) == NULL) {
        yerr("Can't open");
        exit(EXIT_FAILURE);
    }

    file_size = st.st_size;
    buffer = malloc(file_size + 1);
    szread = fread(buffer, sizeof(char), st.st_size, fp);
    fclose(fp);
    if (szread == file_size) {
        buffer[st.st_size] = '\0';
        fprintf(stream, "%s", buffer);
    }
    else
        retcode = EXIT_FAILURE;
    free(buffer);
    exit(retcode);
}
