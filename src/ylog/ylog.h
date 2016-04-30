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

/* A simple logging system for C, inspired by the one in Rust.
 *
 * This library has the following goals/features:
 * - Convenient logging with ylog(level, "format string", <args>)
 * - Run-time and per-file configuration of which log levels to enable
 * - Any calls to a logging function with a level below the one configured
 *   (i.e., a ylog() call that does not result in something being logged)
 *   should have *very* minimal run-time overhead.
 *
 * The above features should encourage adding logging and debugging statements
 * at any place in the code, without having to remove them later on because
 * your application would otherwise generate too much logging output or slow
 * down significantly with calls to logging functions that end up getting
 * ignored.
 *
 *
 * Usage:
 *
 *   #include <ylog.h>
 *
 *   int main(int argc, char **argv) {
 *     // ...
 *
 *     ylog_set_level(YLOG_DEFAULT, getenv("YLOG_LEVEL"));
 *
 *     yinfo("Hello World!");
 *     ydebug("Program name = %s", argv[0]);
 *     // ...
 *   }
 *
 * Check out the function and macros below for details.
 *
 * If logging functions in your application can be called from multiple
 * threads, be sure to compile ylog.c with thread-safety enabled:
 *   -DYLOG_PTHREAD
 *
 * This library uses the fnmatch() function, available on every POSIX OS.  If
 * you want to use this library on Windows, you can probably find an
 * implementation of fnmatch() somewhere (musl-libc has a fairly simple
 * implementation that should be trivially portable to windows).
 *
 *
 * Caveats:
 *
 *  - A static variable may not be visible only in a single file
 *    > Calling a debugging function from an #include'd file (e.g. in a static
 *      inline function) may cause the matching feature to match the wrong file
 *      names.
 *    > Similar issues with the #line directive.
 *
 *  - __FILE__ is not very reliable
 *    > If the build system chdir()s before calling the compiler,
 *      "dir1/file.c" and "dir2/file.c" will both have __FILE__ == "file.c".
 *    > If the build system supports out-of-source builds, any arbitrary dir
 *      may be prefixed. E.g. dir1/file.c could have a __FILE__ of
 *      "../../someproject/src/dir1/file.c"
 *      (The pattern matching algorithm takes this into account)
 *    > If ylog is used in a library, then a file in the lib with the same name
 *      as a file in another lib or in the program will have the same __FILE__.
 *      (But see below for a possible solution)
 *
 *  - There must only be a single ylog.o linked into a program.
 *    > If a program and some linked library and both use ylog, there must be
 *      some way to ensure that ylog.o is not duplicated.
 *      (Easiest way to do this is to put ylog into a separate shared object,
 *      but that's kinda overkill...)
 *    > Preferable solution (IMO): Rename the public ylog_ functions within a
 *      library to have a different prefix, and expose the _set_handler() and
 *      _set_level() functions to the application. (This also allows the
 *      application to differentiate between log entries from itself and from
 *      the lib, solving the unreliable __FILE__ problem in that case).
 */

#ifndef YLOG_H
#define YLOG_H

#include <stdarg.h>
#include <stdlib.h>


/* Pre-defined log levels. You can of course use your own, as long as they are
 * within the range of 0 (including) to YLOG_MAX (including). */
#ifndef YLOG_ERR
#define YLOG_ERR   1
#endif
#ifndef YLOG_WARN
#define YLOG_WARN  2
#endif
#ifndef YLOG_INFO
#define YLOG_INFO  3
#endif
#ifndef YLOG_DEBUG
#define YLOG_DEBUG 4
#endif
#ifndef YLOG_TRACE
#define YLOG_TRACE 5
#endif

#define YLOG_MAX 9999

/* Default log level if ylog_set_level() hasn't been called yet. */
#define YLOG_DEFAULT YLOG_WARN

/* Special number to indicate that the log level for this file hasn't been
 * determined yet. */
#define YLOG_UNINITIALIZED (YLOG_MAX+1)


/* A file-local structure. Each source file is identified with this structure. */
typedef struct ylog_file_t ylog_file_t;
struct ylog_file_t {
	/* Current log level used for this file. Stored in this struct to allow for
	 * quickly determining whether or not a certain statement should be logged
	 * or not, without incurring the overhead of a function call (and possibly
	 * any matching algorithm). */
	int level;
	/* Name of this file (only known after initialization). */
	const char *name;
	/* Pointer to the next file, this creates a linked list of all source files
	 * that have attempted to log something, and allows for dynamically
	 * adjusting log level configuration. */
	ylog_file_t *next;
};


#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#define YLOG_ATTR_PRINTF(fmt, arg) __attribute__((__format__ (__printf__, fmt, arg)))
#define YLOG_ATTR_UNUSED __attribute__((__unused__))
#else
#define YLOG_ATTR_PRINTF(fmt, arg)
#define YLOG_ATTR_UNUSED
#endif


/* Allow overwriting the logging filename without using #line. */
#ifndef YLOG_FILE_NAME
#define YLOG_FILE_NAME __FILE__
#endif


/* The file-local structure. */
static ylog_file_t ylog_file YLOG_ATTR_UNUSED = { YLOG_UNINITIALIZED, NULL, NULL };


/* Internal function implementing the ylog() macro. You shouldn't need to call
 * this directly. */
void ylog_impl(ylog_file_t *file, int level, int line, const char *filename, const char *fmt, ...) YLOG_ATTR_PRINTF(5, 6);


/* Returns false if a log entry at the given level will not be logged. Useful
 * when you can avoid extra processing overhead that is only necessary for
 * constructing the log entry, e.g.
 *
 *   if(ylog_enabled(YLOG_DEBUG)) {
 *     char *extra_info = get_debugging_info();
 *     ydebug("get_debugging_info = %s", extra_info);
 *     free(extra_info);
 *   }
 *
 * Note that this macro may still return true even if the entry will NOT be
 * logged, it should therefore not be used for any other situations than
 * described above.
 */
#define ylog_enabled(lvl) ((lvl) <= ylog_file.level)


/* First argument may be evaluated more than once. */
#define ylog(lvl, ...) do {\
		if(ylog_enabled(lvl))\
			ylog_impl(&ylog_file, (lvl), __LINE__, YLOG_FILE_NAME, __VA_ARGS__);\
	} while(0)


#define yerr(...)   ylog(YLOG_ERR,   __VA_ARGS__)
#define yerror(...) ylog(YLOG_ERR,   __VA_ARGS__)
#define ywarn(...)  ylog(YLOG_WARN,  __VA_ARGS__)
#define yinfo(...)  ylog(YLOG_INFO,  __VA_ARGS__)
#define ydebug(...) ylog(YLOG_DEBUG, __VA_ARGS__)
#define ytrace(...) ylog(YLOG_TRACE, __VA_ARGS__)


typedef void(*ylog_handler_cb)(const char *file, int line, int level, const char *message);


/* A default log handler, which forwards everything to stderr. */
void ylog_default_handler(const char *file, int line, int level, const char *message);


/* Set the callback that receives all (enabled) log entries. The callback will
 * be run in the same thread and context as the code calling the log function.
 * In a multi-threaded application, this means that it can be called from
 * multiple threads at the same time. This also means that it can insert a
 * breakpoint/abort/trap to allow for debugging at certain log messages.
 *
 * In a multi-threaded application, it is not guaranteed that the previously
 * set callback will not be called anymore after this function has returned,
 * only that new invocations of ylog() will use the new callback.
 *
 * IMPORTANT: Do *NOT* call any ylog() function from a handler. It may result
 * in infinite recursion.
 */
void ylog_set_handler(ylog_handler_cb cb);


/* Set the log level configuration. 'patterns' is a comma-separated list of
 * patterns, describing the log level used for individual files. The format is
 * as follows:
 *
 * 	 <pattern_1>:<level_1>,<pattern_2>:<level_2>,...,<default_level>
 *
 * Where <pattern_n> is a file-name pattern given to fnmatch(), and <level_n> a
 * decimal number indicating the log level used for files that match the
 * pattern. The list of patterns is traversed in the order given in the list,
 * the first matching pattern is used. A <default_level> can be specified at
 * the end of the pattern, indicating the log level used when none of the
 * previous patterns matched.  The .c, .h, .cc, .cpp, and .hpp file extensions
 * are removed from the file name before matching, and any directory prefixes
 * not mentioned in the pattern are ignored. Examples:
 *
 *   "3" or "*:3"
 *     Use log level 3 for all files.
 *
 *   "main:1,2"
 *     Use 1 as log level for main.(c,h,cc,cpp,hpp) in any directory,
 *     Use 2 for all other files.
 */
//   "*_util:4,net/*:3,9"
//     Use 4 for any filename ending with _util.(c,h,cc,cpp,hpp) in any
//           directory.
//     Use 3 for any file in a net/ directory, not matching *_util,
//           (but not in a subdirectory, that adding the net/*/* pattern)
//     Use 9 for all other files.
/*
 * If no pattern exists for a file (and there is no final catch-all pattern, as
 * in the examples above), default_level will be used instead. default_level
 * may also be used if the pattern string does not follow the above format.
 * This function overrides any previously used configuration.
 */
void ylog_set_level(int default_level, const char *patterns);

#endif

/* vim: set noet sw=4 ts=4: */
