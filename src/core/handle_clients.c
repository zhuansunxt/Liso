#include "handle_clients.h"

client_pool pool;
client_pool *p = &pool;


/*
 * Initialize client pool with <listenfd> as the only active socket descriptor.
 * After init, client pool is empty with only one listener socket and is ready
 * for accepting client requests.
 */
void init_pool(int listenfd) {
  FD_ZERO(&p->master);
  FD_ZERO(&p->read_fds);
  FD_SET(listenfd, &p->master);
  p->maxfd = listenfd;
  p->nready = 0;
  p->maxi = -1;
  int i;
  for (i = 0; i < FD_SETSIZE; i++) {
    p->client_fd[i] = -1;     /* - 1 indicates avaialble entry */
    p->client_buffer[i] = NULL;
    p->buffer_offset[i] = -1;
    p->buffer_cap[i] = 0;
    p->received_header[i] = 0;
  }
}

/*
 * Add new client socket descriptor to client pool.
 */
void add_client_to_pool(int newfd) {
  int i;
  p->nready--;    /* listener socket */

  for (i = 0; i < FD_SETSIZE; i++) {
    if (p->client_fd[i] < 0) {
      /* Find a available entry */
      p->client_fd[i] = newfd;

      /* Initialize client buffer */
      p->client_buffer[i] = (char *)malloc(sizeof(char)*BUF_SIZE);
      memset(p->client_buffer[i], 0, sizeof(char)*BUF_SIZE);
      p->buffer_offset[i] = 0;
      p->buffer_cap[i] = sizeof(char)*BUF_SIZE;

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

void handle_clients() {
  ssize_t  nbytes;
  int i, clientfd;
  char buf[BUF_SIZE];

  for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
    clientfd = p->client_fd[i];
    if (clientfd <= 0) continue;

    memset(buf, 0, sizeof(char)*BUF_SIZE);

    /* If client is ready, read request from it and echo it back */
    if (FD_ISSET(clientfd, &p->read_fds)) {
      set_fl(clientfd, O_NONBLOCK);     /* set nonblocking */
      nbytes = recv(clientfd, buf, BUF_SIZE, 0);

      if (nbytes > 0) {
        dump_log("[Client pool] Receive %d bytes from client on socket %d", nbytes, clientfd);

        char *client_buffer = append_request(clientfd, buf, nbytes);

        char *header_end_id = "\r\n\r\n";
        char *header_end = strstr(client_buffer, header_end_id);
        size_t header_len = get_client_buffer_offset(clientfd);
        if (header_end == NULL && header_len < BUF_SIZE) continue;   /* HTTP header not complete yet */

        set_header_received(clientfd);
        int http_res = handle_http_request(clientfd, client_buffer, header_len);
        console_log("[INFO] Client %d request result %d", clientfd, http_res);

        if (http_res > 0) {
          clear_client(clientfd);
        } else if (http_res == 0) {
          /* TODO: keep client's state and expect more info */
        } else if (http_res < 0) {
          /* TODO: handle internal server error when handling http request */
        }

        //clr_fl(clientfd, O_NONBLOCK);   /* clear nonblocking */
      }
      else if (nbytes <= 0) {
        if (nbytes == 0) {    /* Connection closed by client */
          dump_log("[Client pool] Connection closed by client on socket %d", clientfd);
        } else {
          if (errno == EINTR) continue;
          dump_log("[Client pool] Error on recv() from client on socket %d", clientfd);
        }
        clear_client_by_idx(clientfd, i);
      }

      /* This minus one operation should be put here, and is the only
       * place nready is substracted: after each loop iteration, current client
       * should be removed from master set and be selected again. */
      p->nready--;
    }

  } // End for loop.
} // End function.

/*
 * Remove client from pool and release all corrsponding resources.
 */
void clear_client_by_idx(int client_fd, int idx){
  close(client_fd);
  FD_CLR(client_fd, &p->master);
  p->client_fd[idx] = -1;
  free(p->client_buffer[idx]);
  p->buffer_offset[idx] = -1;
  p->buffer_cap[idx] = 0;
  p->received_header[idx] = 0;
}

/* Clear resource associated with client */
void clear_client(int client) {
  int idx, cliendfd;

  for (idx = 0; idx < FD_SETSIZE; idx++) {
    cliendfd = p->client_fd[idx];
    if (cliendfd == client) {
      /* Free resources and reset all states */
      close(cliendfd);
      FD_CLR(cliendfd, &p->master);
      p->client_fd[idx] = -1;
      free(p->client_buffer[idx]);
      p->buffer_offset[idx] = -1;
      p->buffer_cap[idx] = 0;
      p->received_header[idx] = 0;
      return ;
    }
  }
  // no such client to be found
#ifdef DEBUG_VERBOSE
  console_log("[INFO] Client not found when clearing client %d", client);
#endif
  dump_log("[INFO] Client not found when clearing client %d", client);
}

/*
 * Clear pool. Only call this when server crash.
 */
void clear_pool() {
  int i, clientfd;
  for (i = 0; i < FD_SETSIZE; i++) {
    clientfd = p->client_fd[i];
    if (clientfd > 0) clear_client_by_idx(clientfd, i);
  }
}

/*
 * Append received content to client's buffer
 */
char* append_request(int client, char *buf, ssize_t len) {
  int i,clientfd;
  int expansion;

  for (i = 0; (i <= p->maxi) && p->nready > 0; i++) {
    clientfd = p->client_fd[i];
    if (clientfd == client) {
      size_t offset = p->buffer_offset[i];
      size_t current_capacity = p->buffer_cap[i];

#ifdef DEBUG_VERBOSE
      console_log("[INFO] Trying to append request to client buffer...");
      console_log("[INFO] Client %d buffer capacity %d", clientfd, current_capacity);
      console_log("[INFO] Client %d need buffer space %d (%d offset + %d nbytes)",
                  clientfd, (offset+len), offset, len);
#endif

      /* dynamically allocate space */
      if ((offset+len) > current_capacity) {
        /* Determine expansion:  keep doubling current capacity until the expansion size
           is larger than the size of needed space, which is (offset+len). */
        expansion = current_capacity;
        while ((offset+len) > expansion)  expansion *= 2;

        p->client_buffer[i] = realloc(p->client_buffer[i], expansion);
        if (p->client_buffer[i] == NULL) {
          err_sys("Memory can not be allocated");
        }
        p->buffer_cap[i] = expansion;
#ifdef DEBUG_VERBOSE
        console_log("[INFO] Client %d buffer capacity expands from %d bytes to %d bytes",
                    clientfd, current_capacity, expansion);
#endif
      }

      strncpy(p->client_buffer[i]+offset, buf, len);
      p->buffer_offset[i] += len;
      return p->client_buffer[i];
    }
  }
  // no such client to be appended
#ifdef DEBUG_VERBOSE
  console_log("[INFO] Client not found when trying to append request content");
#endif
  dump_log("[INFO] Client not found when trying to append request content");
  return NULL;
}

/*
 * Get client buffer's offset, which is current length of request content.
 */
size_t get_client_buffer_offset(int client) {
  int i;
  int clientfd;
  for (i = 0; (i <= p->maxi) && p->nready > 0; i++) {
    clientfd = p->client_fd[i];
    if (clientfd == client) return p->buffer_offset[i];
  }
  // no such client to be found
#ifdef DEBUG_VERBOSE
  console_log("[INFO] Client not found when trying to get buffer offset");
#endif
  dump_log("[INFO] Client not found when trying to get buffer offset");
  return 0;
}

/*
 * set a client's state from <header not received> to <header received>
 */
void set_header_received(int client) {
  int i, clientfd;

  for(i = 0; i < FD_SETSIZE; i++) {
    clientfd = p->client_fd[i];
    if (clientfd == client) {
      p->received_header[i] = 1;
      return ;
    }
  }
  // no such client to be found
#ifdef DEBUG_VERBOSE
  console_log("[INFO] Client not found when setting received header to be true");
#endif
  dump_log("[INFO] Client not found when setting received header to be true");
}