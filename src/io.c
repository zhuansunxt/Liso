#include "io.h"

/*
 * Get sockaddr, IPv4 or IPv6.
 */
void *get_in_addr(sockaddr *sa){
  if (sa->sa_family == AF_INET) {
    return &(((sockaddr_in *)sa)->sin_addr);
  }
  return &(((sockaddr_in6 *)sa)->sin6_addr);
}

/*
 * Open and return listener socket on given port.
 * Return socket descriptor on success.
 * Return -1 and set errno on failure.
 */
int open_listenfd(int port){
  int listenfd;
  int yes = 1;
  sockaddr_in serveraddr;

  /* Create socket descriptor for listening usage */
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printlog("%s create listener socket", SOCKET_API_ERR_MSG);
    return -1;
  }

  /* Eliminates "Address already in use" error from bind */
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
    printlog("%s set SO_REUSEADDR", SOCKET_API_ERR_MSG);
    return -1;
  }

  /* Bind listener socket */
  memset(&serveraddr, 0, sizeof(sockaddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)port);
  if (bind(listenfd, (sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
    printlog("%s bind listenser socket", SOCKET_API_ERR_MSG);
    close(listenfd);
    return -1;
  }

  if (listen(listenfd, 10) < 0) {
    printlog("%s listen on listener socket", SOCKET_API_ERR_MSG);
    return -1;
  }

  return listenfd;
}

/*
 * Wrapper for send(): send n bytes
 * guarantee to send all data given length n.
 * Return number of bytes that are sent on success.
 * Return -1 on error.
 */
int sendn(int fd, const void *vptr, int n) {
  size_t nleft = n;
  ssize_t nwritten = 0;
  const char *ptr = vptr;

  while (nleft > 0) {
    if ((nwritten = send(fd, ptr+nwritten, n, 0)) < 0) {
      if (nwritten < 0 && errno == EINTR) {
        nwritten = 0;   /* ignore the interrupt */
      } else {
        return -1;    /* error */
      }
    }

    nleft -= nwritten;
    ptr += nwritten;
  }

  return n;
}

/*
 * Wrapper for sendn
 * Will termiate when error in system call.
 */
void Sendn(int fd, const void *ptr, int n) {
  if (sendn(fd, ptr, n) != n)
    err_sys("sendn error");
}
