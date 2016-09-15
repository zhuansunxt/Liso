
#include "utilities.h"

void printlog(const char *fmt, ...){
#if DEBUG_VERBOSE
  va_list(args);
  va_start(args, fmt);
  vprintf(fmt, args);
  printf("\n");
  va_end(args);
#endif
}

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


