#include "utilities.h"

char *LOGFILE;

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
 * Write log to log file, specified by command-line argument.
 * Use this logging to record server runtime info, both for debugging and production.
 */
void write_log(const char *fmt, ...){
  FILE *logfile;
  logfile = fopen(LOGFILE, "a");
  if (logfile == NULL) {
    err_sys("[ERROR] log file cannot be accessed");
    return;
  }

  va_list(args);
  va_start(args, fmt);
  vfprintf(logfile, fmt, args);
  va_end(args);
  fprintf(logfile, "\n");
  fflush(logfile);
  fclose(logfile);
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
  int		val;

  if ((val = fcntl(fd, F_GETFL, 0)) < 0)
    err_sys("fcntl F_GETFL error");

  val |= flags;		/* turn on flags */


  if (fcntl(fd, F_SETFL, val) < 0)
    err_sys("fcntl F_SETFL error");
}

void clr_fl(int fd, int flags)
/* flags are the file status flags to turn off */
{
  int		val;

  if ((val = fcntl(fd, F_GETFL, 0)) < 0)
    err_sys("fcntl F_GETFL error");

  val &= ~flags;		/* turn flags off */

  if (fcntl(fd, F_SETFL, val) < 0)
    err_sys("fcntl F_SETFL error");
}


