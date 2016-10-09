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
    WAITING_FOR_CGI,
    CGI_FOR_READ,
    CGI_FOR_WRITE
} client_state;

typedef enum client_type {
    HTTP_CLIENT,
    HTTPS_CLIENT
} client_type;

typedef struct {
  fd_set master;              /* all descritors */
  fd_set read_fds;            /* all ready-to-read descriptors */
  fd_set write_fds;           /* all ready-to_write descriptors */
  int maxfd;                  /* maximum value of all descriptors */
  int nready;                 /* number of ready descriptors */
  int maxi;                   /* maximum index of available slot */
  int client_fd[FD_SETSIZE];  /* client slots */

  /* State */
  dynamic_buffer * client_buffer[FD_SETSIZE]; /* client's dynamic-size buffer */
  size_t received_header[FD_SETSIZE];         /* store header ending's offset */
  client_state state[FD_SETSIZE];             /* client's state */
  char should_be_close[FD_SETSIZE];           /* whether client should be closed when checked */

  /* Type */
  client_type type[FD_SETSIZE];               /* client's type: HTTP or HTTPS */
  SSL * context[FD_SETSIZE];                  /* set if client's type is HTTPS */

  char *remote_addr[FD_SETSIZE];

  /* CGI related */
  int cgi_client[FD_SETSIZE];
} client_pool;

extern client_pool pool;

/* -------- Client pool APIs -------- */
void init_pool();
void add_client_to_pool(int newfd, SSL*, client_type, char*);
void add_cgi_fd_to_pool(int clientfd, int cgi_fd, client_state state);
void handle_clients();
void clear_client_by_idx(int, int);
void clear_client(int);
void clear_pool();
int get_client_index(int client);
void reset_client_buffer_state_by_idx(int, int);
dynamic_buffer* get_client_buffer_by_client(int);
void print_pool();

size_t get_client_buffer_offset(int);
void set_header_received(int, size_t);

size_t handle_recv_header(int, char *);

#endif //LISO_SERVER_H
