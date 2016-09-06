
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

/* Logging */
void printlog(const char *fmt, ...);

/* Error handling */
void err_sys(const char *fmt, ...);

/* Non-block IO */
void set_fl(int fd, int flags);
void clr_fl(int fd, int flags);

#endif //LISO_UTILITIES_H
