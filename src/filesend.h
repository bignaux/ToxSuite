#ifndef FILESEND_H
#define FILESEND_H
#include <time.h>
#include <stdint.h>

#include "fileop.h"
#include "misc.h"

#define FSEND_TIMEOUT 300

enum {
	FILESEND_OK,
	FILESEND_ERR_FULL,
	FILESEND_ERR_SENDING,
	FILESEND_ERR_FILEIO,
	FILESEND_ERR
};

int file_sender_new(uint32_t friend_number, FileNode **shrfiles, int packn, Tox *m);
extern char packlist_filename[];

#endif
