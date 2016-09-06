//
// Created by XiaotongSun on 16/9/5.
//

#ifndef LISO_SERVER_H
#define LISO_SERVER_H

#include "io.h"

typedef struct {
  fd_set master;              /* all descritors */
  fd_set read_fds;            /* all ready-to-read descriptors */
  int maxfd;                  /* maximum value of all descriptors */
  int nready;                 /* number of ready descriptors */
  int maxi;                   /* maximum index of available slot */
  int client_fd[FD_SETSIZE];  /* client slots */
} client_pool;

void init_pool(int listenfd, client_pool *p);
void add_client_to_pool(int newfd, client_pool *p);
void handle_clients(client_pool *p);
void clear_client(int clientfd, int idx, client_pool*p);

#endif //LISO_SERVER_H
