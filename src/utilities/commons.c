/*
 * Utilities:
 *  This file contains all utility functions what may be used across the whole system.
 *  Any function that are widely needed, should goes to this file.
 */
#include "commons.h"

char *LOGFILE;
FILE *log_file;

/*
 * Write log to console.
 * Use this logging to output interactive info.
 */
void console_log(const char *fmt, ...){
  va_list(args);
  va_start(args, fmt);
  vprintf(fmt, args);
  printf("\n");
  va_end(args);
}

/*
 * Initiate log file.
 * Return 0 on success, return 1 on failure.
 */
int init_log() {
  int log_exist;
  if ((log_exist = access(LOGFILE, R_OK)) == 0) {
    remove(LOGFILE);
    console_log("[INFO] log file exists, force delete it");
  }

  log_file = fopen(LOGFILE, "a");
  if (log_file == NULL) {
    err_sys("[ERROR] log file can not be created");
    return -1;
  }
  return 0;
}

/*
 * Write log to log file, specified by command-line argument.
 * Use this logging to record server runtime info, both for debugging and production.
 */
void dump_log(const char *fmt, ...){
  va_list(args);
  va_start(args, fmt);
  vfprintf(log_file, fmt, args);
  va_end(args);
  fprintf(log_file, "\n");
  fflush(log_file);
}

/*
 * Close log file.
 */

void close_log() {
  fclose(log_file);
}


/*
 * Output system error to console to indicate system call failure
 */
void err_sys(const char *fmt, ...){
  va_list(args);
  va_start(args, fmt);
  vprintf(fmt, args);
  printf("\n");
  va_end(args);
  exit(1);
}

void set_fl(int fd, int flags) /* flags are file status flags to turn on */
{
  int val;

  if ((val = fcntl(fd, F_GETFL, 0)) < 0)
    err_sys("fcntl F_GETFL error");

  val |= flags;		/* turn on flags */


  if (fcntl(fd, F_SETFL, val) < 0)
    err_sys("fcntl F_SETFL error");
}

void clr_fl(int fd, int flags)
/* flags are the file status flags to turn off */
{
  int val;

  if ((val = fcntl(fd, F_GETFL, 0)) < 0)
    err_sys("fcntl F_GETFL error");

  val &= ~flags;		/* turn flags off */

  if (fcntl(fd, F_SETFL, val) < 0)
    err_sys("fcntl F_SETFL error");
}
