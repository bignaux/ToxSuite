#ifndef FILE_H
#define FILE_H

// UINT32_MAX
#define NUM_FILE_SENDERS 64
#define STRING_LENGTH 256
#define HISTORY 50

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

#include <tox/tox.h>
//#include <tox/toxav.h>
#include <sodium.h>

#include "ylog/ylog.h"
#include "list.h"

/* "info" dictionnary */
typedef struct FileNode {
    char *file; //TODO rename "name" for client
    uint8_t *BLAKE2b; // binary TOX_FILE_ID_LENGTH blake2b hash;
    time_t mtime;
    off_t length;
    int exists;
} FileNode;

typedef struct FileSender{
    /* shared informations */

    FileNode *info; //TODO : array with multiple files, nested ?

    /* core information */
    FILE *file;
    uint32_t file_number;
    uint32_t friend_number;

    /* Tox-xd */
    uint8_t accepted;
    time_t timestamp;

    /* TS */
    // TODO : bool erase_after_send
    char *pathname; // currently /full/path/filename
//    char *filename;
    uint32_t kind;
//    uint64_t file_size;
//    uint8_t *file_id;
    struct list_head list;
} FileSender;

struct list_head FilesSender;

/* operation on Queue */
void FileQueue_init(struct list_head *FileQueue);
void FileQueue_destroy(struct list_head *FileQueue);
int FileQueue_size(struct list_head *FileQueue);
FileSender *FileSender_get(struct list_head *FileQueue, const uint32_t friend_number, const uint32_t file_number);
FileSender *FileSender_new(struct list_head *FileQueue);
void FileSender_destroy(struct FileSender *f);

int printf_file(FILE* stream, const char *filename);


void file_recv_cb(Tox *tox, uint32_t friend_number, uint32_t file_number, uint32_t type, uint64_t file_size,
                         const uint8_t *filename, size_t filename_length, void *user_data);
void file_recv_control_cb(Tox *tox, uint32_t friend_number, uint32_t file_number, TOX_FILE_CONTROL control,
                        void *user_data);
void file_recv_chunk_cb(Tox *tox, uint32_t friend_number, uint32_t filenumber, uint64_t position, const uint8_t *data,
                size_t length, void *user_data);

/*******************************************************************************
 *
 * :: File transmission: sending
 *
 ******************************************************************************/

/* 3 steps :
 * -1-  get a new FileSender f with f = FileSender_new()
 * -2-  feed it with minimal requiremnt ( open file pointer + friend_number)
 * -3-  give it to add_filesender(f)
 */
uint32_t add_filesender(Tox *m, FileSender *f);

void file_chunk_request_cb(Tox *tox, uint32_t friend_number, uint32_t file_number, uint64_t position, size_t length,
                            void *user_data);

#endif
