
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

//#define DEBUG_VERBOSE 1
#define SERVER_FAILURE 1   /* main execution returns this value when server crash */
#define BUF_SIZE 8192

char *LOGFILE;
FILE *log_file;

/*
 * logging utilities
 */
void console_log(const char *fmt, ...);
int init_log();
void dump_log(const char *fmt, ...);
void close_log();

/*
 * error handling
 */
void err_sys(const char *fmt, ...);
void tear_down();

/* Non-block IO */
void set_fl(int fd, int flags);
void clr_fl(int fd, int flags);

#endif //LISO_UTILITIES_H
