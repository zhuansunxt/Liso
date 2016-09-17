
#ifndef LISO_UTILITIES_H
#define LISO_UTILITIES_H

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEBUG_VERBOSE 0
#define SERVER_FAILURE 1   /* main execution returns this value when server crash */

extern char *LOGFILE;
extern FILE *log_file;

/*
 * logging utilities
 */
void console_log(const char *fmt, ...);
int init_log();
void dump_log(const char *fmt, ...);
void close_log();

/*
 * error handling utilites
 */
void err_sys(const char *fmt, ...);

/* Non-block IO */
void set_fl(int fd, int flags);
void clr_fl(int fd, int flags);

#endif //LISO_UTILITIES_H
