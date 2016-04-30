/* Copyright (c) 2012 Yoran Heling

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "ylog.h"
#include <stdio.h>
#include <string.h>
#include <fnmatch.h>


/* Maximum message length to allocate on the stack. */
#ifndef YLOG_STACK_MSG_SIZE
#define YLOG_STACK_MSG_SIZE 256
#endif


#ifdef YLOG_PTHREAD
#include <pthread.h>
static pthread_mutex_t ylog_mutex = PTHREAD_MUTEX_INITIALIZER;
#define mutex_lock()   pthread_mutex_lock(&ylog_mutex);
#define mutex_unlock() pthread_mutex_unlock(&ylog_mutex)

#else
#define mutex_lock()
#define mutex_unlock()
#endif


/* Global config. */
static ylog_file_t     *ylog_files         = NULL;
static int              ylog_default_level = YLOG_DEFAULT;
static char            *ylog_pattern       = NULL;
static ylog_handler_cb  ylog_handler       = ylog_default_handler;


/* Returns 1 on match, 0 otherwise. */
static int ylog_match(const char *pat, const char *fn) {
	const char *f;
	if(!fnmatch(pat, fn, 0))
		return 1;
	for(f=fn; *f; f++)
		if(*f == '/' && f[1] != '/' && !fnmatch(pat, f+1, 0))
			return 1;
	return 0;
}


/* Assumes the mutex is locked */
static void ylog_set_file_level(ylog_file_t *file) {
	char *start, *pat = ylog_pattern;
	int val;

	while(pat && *pat) {
		start = pat;
		while(*pat && *pat != ':' && *pat != ',')
			pat++;
		/* No colon, assume that this is the <default_level> */
		if(*pat != ':') {
			pat = start;
			break;
		}
		*pat = 0;
		val = ylog_match(start, file->name);
		*(pat++) = ':';
		if(val)
			break;
		while(*pat && pat[-1] != ',')
			pat++;
	}

	/* Early break, this means that we expect the numeric level for this string. */
	if(pat && *pat) {
		start = pat;
		val = 0;
		while(val < YLOG_MAX && *pat >= '0' && *pat <= '9') {
			val = val*10 + (*pat-'0');
			pat++;
		}
		if(val <= YLOG_MAX && pat != start && (!*pat || *pat == ',')) {
			file->level = val;
			return;
		}
	}

	file->level = ylog_default_level;
}


/* Removes the file extension from the file name and sets file->name. */
static void ylog_set_file_name(ylog_file_t *file, const char *filename) {
	char *f;
	int l = strlen(filename);

	f = malloc(l+1);
	/* Just use the unmodified static string if malloc failed. */
	if(!f) {
		file->name = filename;
		return;
	}
	memcpy(f, filename, l+1);

	/* Stripped file extensions: .c, .h, .cc, .cpp, .hpp.
	 * TODO: This code is ugly, a cleanup would be nice. */
	if(l > 2 && (f[l-1] == 'c' || f[l-1] == 'h') && f[l-2] == '.') /* .c, .h */
		f[l-2] = 0;
	else if(l > 3 && f[l-1] == 'c' && f[l-2] == 'c' && f[l-3] == '.') /* .cc */
		f[l-3] = 0;
	else if(l > 4 && f[l-1] == 'p' && f[l-2] == 'p' && (f[l-3] == 'c' || f[l-3] == 'h') && f[l-4] == '.') /* .cpp, .hpp */
		f[l-4] = 0;

	file->name = f;
}


void ylog_impl(ylog_file_t *file, int level, int line, const char *filename, const char *fmt, ...) {
	va_list ap;
	ylog_handler_cb cb;
	int l;
	char msg_stack[YLOG_STACK_MSG_SIZE];
	char *msg = msg_stack;

	/* Init */
	mutex_lock();
	if(file->level == YLOG_UNINITIALIZED) {
		ylog_set_file_name(file, filename);
		ylog_set_file_level(file);
		file->next = ylog_files;
		ylog_files = file;
		if(level > file->level) {
			mutex_unlock();
			return;
		}
	}
	/* Make a copy of the callback, so that we don't have the mutex locked
	 * while formatting the message and running the callback */
	cb = ylog_handler;
	mutex_unlock();

	/* Format & call */
	va_start(ap, fmt);
	l = vsnprintf(msg_stack, YLOG_STACK_MSG_SIZE, fmt, ap);
	va_end(ap);
	if(l >= YLOG_STACK_MSG_SIZE) {
		msg = malloc(l+1);
		if(!msg) /* Ouch... */
			return;
		va_start(ap, fmt);
		vsnprintf(msg, l+1, fmt, ap);
		va_end(ap);
	}
	cb(filename, line, level, msg);
	if(msg != msg_stack)
		free(msg);
}


void ylog_set_handler(ylog_handler_cb cb) {
	mutex_lock();
	ylog_handler = cb;
	mutex_unlock();
}


/* TODO: Validate the pattern? */
void ylog_set_level(int default_level, const char *pattern) {
	ylog_file_t *f;

	mutex_lock();

	ylog_default_level = default_level;
	if(ylog_pattern)
		free(ylog_pattern);
	ylog_pattern = pattern ? malloc(strlen(pattern)+1) : NULL;
	if(ylog_pattern) /* XXX: malloc() error is silently ignored :( */
		strcpy(ylog_pattern, pattern);

	for(f=ylog_files; f; f=f->next)
		ylog_set_file_level(f);

	mutex_unlock();
}


void ylog_default_handler(const char *file, int line, int level, const char *message) {
	char lvl[8];
	switch(level) {
	case YLOG_ERR:   strcpy(lvl, "ERROR"); break;
	case YLOG_WARN:  strcpy(lvl, "WARN");  break;
	case YLOG_INFO:  strcpy(lvl, "info");  break;
	case YLOG_DEBUG: strcpy(lvl, "debug"); break;
	case YLOG_TRACE: strcpy(lvl, "trace"); break;
	default:
		sprintf(lvl, "%d", level);
	}
	mutex_lock();
	fprintf(stderr, "--%s-- %s:%d: %s\n", lvl, file, line, message);
	mutex_unlock();
}

/* vim: set noet sw=4 ts=4: */
