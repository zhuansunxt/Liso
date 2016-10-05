//
// Created by XiaotongSun on 16/9/5.
//

#ifndef LISO_SERVER_H
#define LISO_SERVER_H

#include "../utilities/io.h"
#include "handle_request.h"

typedef enum client_state {
    READY_FOR_READ,
    READY_FOR_WRITE,
} client_state;

typedef struct {
  fd_set master;              /* all descritors */
  fd_set read_fds;            /* all ready-to-read descriptors */
  fd_set write_fds;           /* all ready-to_write descriptors */
  int maxfd;                  /* maximum value of all descriptors */
  int nready;                 /* number of ready descriptors */
  int maxi;                   /* maximum index of available slot */
  int client_fd[FD_SETSIZE];  /* client slots */

  /* State */
  dynamic_buffer * client_buffer[FD_SETSIZE];
  size_t received_header[FD_SETSIZE];  /* store header ending's offset */
  client_state state[FD_SETSIZE];
  char should_be_close[FD_SETSIZE];
} client_pool;

extern client_pool pool;

/* -------- Client pool APIs -------- */
void init_pool(int listenfd);
void add_client_to_pool(int newfd);
void handle_clients();
void clear_client_by_idx(int, int);
void clear_client(int);
void clear_pool();
void reset_client_buffer_state_by_idx(int, int);
void print_pool();

char* append_request(int, char *, ssize_t);
size_t get_client_buffer_offset(int);
void set_header_received(int, size_t);

size_t handle_recv_header(int, char *);

#endif //LISO_SERVER_H
