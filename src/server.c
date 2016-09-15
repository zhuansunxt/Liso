#include "server.h"

const unsigned int BUF_SIZE = 8192;

/*
 * Initialize client pool with <listenfd> as the only active socket descriptor.
 * After init, client pool is empty with only one listener socket and is ready
 * for accepting client requests.
 */
void init_pool(int listenfd, client_pool *p) {
  FD_ZERO(&p->master);
  FD_ZERO(&p->read_fds);
  FD_SET(listenfd, &p->master);
  p->maxfd = listenfd;
  p->nready = 0;
  p->maxi = -1;
  int i;
  for (i = 0; i < FD_SETSIZE; i++) {
    p->client_fd[i] = -1;     /* - 1 indicates avaialble entry */
  }
}

/*
 * Add new client socket descriptor to client pool.
 */
void add_client_to_pool(int newfd, client_pool *p) {
  int i;
  p->nready--;    /* listener socket */

  for (i = 0; i < FD_SETSIZE; i++) {
    if (p->client_fd[i] < 0) {
      /* Find a available entry */
      p->client_fd[i] = newfd;
      FD_SET(newfd, &p->master);
      p->maxfd = (newfd > p->maxfd) ? newfd : p->maxfd;
      p->maxi = (i > p->maxi) ? i : p->maxi;
      break;
    }
  }

  if (i == FD_SETSIZE) {
    /* Coundn't find available slot in pool */
    console_log("[Client pool] Too many clients! No available slot for new client connection");
    //TODO: handle too many client case
  }
}

void handle_clients(client_pool *p) {
  ssize_t  nbytes;
  int i, clientfd;
  char buf[BUF_SIZE];

  for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
    clientfd = p->client_fd[i];
    if (clientfd <= 0) continue;

    /* If client is ready, read request from it and echo it back */
    if (FD_ISSET(clientfd, &p->read_fds)) {
      set_fl(clientfd, O_NONBLOCK);     /* set nonblocking */
      nbytes = recv(clientfd, buf, BUF_SIZE, 0);
      if (nbytes > 0) {       /* Successful read, echo msg back */
        write_log("[Client pool] Receive %d bytes from client on socket %d", nbytes, clientfd);
        Sendn(clientfd, buf, nbytes);
        clr_fl(clientfd, O_NONBLOCK);   /* clear nonblocking */
      } else if (nbytes <= 0){
        if (nbytes == 0) {    /* Connection closed by client */
          write_log("[Client pool] Connection closed by client on socket %d", clientfd);
        } else {
          if (errno == EINTR) continue;   /* TODO: reason about this */
          err_sys("error on recv() from client on socket %d", clientfd);
        }
        clear_client(clientfd, i, p);
      }
      p->nready--;
    } // End handling readable descriptor.
  } // End for loop.
} // End function.

/*
 * Remove client from pool and release all corrsponding resources.
 */
void clear_client(int client_fd, int idx, client_pool* p){
  close(client_fd);
  FD_CLR(client_fd, &p->master);
  p->client_fd[idx] = -1;
}
