//
// Created by XiaotongSun on 16/9/5.
//

#ifndef LISO_SERVER_H
#define LISO_SERVER_H

#include "../utilities/io.h"
#include "handle_request.h"

typedef struct {
  fd_set master;              /* all descritors */
  fd_set read_fds;            /* all ready-to-read descriptors */
  int maxfd;                  /* maximum value of all descriptors */
  int nready;                 /* number of ready descriptors */
  int maxi;                   /* maximum index of available slot */
  int client_fd[FD_SETSIZE];  /* client slots */

  /* newly added in CP2 */
  char *client_buffer[FD_SETSIZE];  /* buffer contains client's request content */
  size_t buffer_offset[FD_SETSIZE]; /* current position of client's buffer */
  size_t buffer_cap[FD_SETSIZE];    /* current allocated size of client's buffer */
  char received_header[FD_SETSIZE];  /* if this client's http header not fully received yet */
} client_pool;

extern client_pool pool;

/* -------- Client pool APIs -------- */
void init_pool(int listenfd);
void add_client_to_pool(int newfd);
void handle_clients();
void clear_client(int clientfd, int idx);
void clear_pool();

char* append_request(int, char *, ssize_t);
size_t get_client_buffer_offset(int);
void header_done(int);

#endif //LISO_SERVER_H
