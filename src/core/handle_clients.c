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
      p->client_buffer[i] = (dynamic_buffer*)malloc(sizeof(dynamic_buffer));
      init_dbuffer(p->client_buffer[i]);

      FD_SET(newfd, &p->master);
      p->maxfd = (newfd > p->maxfd) ? newfd : p->maxfd;
      p->maxi = (i > p->maxi) ? i : p->maxi;
      break;
    }
  }

  if (i == FD_SETSIZE) {
    /* Coundn't find available slot in pool */
    console_log("[Client pool] Too many clients! No available slot for new client connection");
  }
}

/* TODO */
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

        append_content_dbuffer(p->client_buffer[i], buf, nbytes);

        /* Check whether received request content has complete HTTP header.
         * If <clrfclrf> is detected, *or* current header_len is larger than
         * BUF_SIZE(8192), we send received accumulated buffer to HTTP handler.*/

        size_t header_recv_offset = handle_recv_header(clientfd, p->client_buffer[i]->buffer);
        if (header_recv_offset == 0) {
          continue;
        } else if (header_recv_offset == -1) {
          clear_client_by_idx(clientfd, i);
          continue;
        }

        set_header_received(clientfd, header_recv_offset);

        int http_res = handle_http_request(clientfd, p->client_buffer[i], header_recv_offset);
        console_log("[INFO] Client %d request result %d", clientfd, http_res);

        if (http_res == 1) {          /* Connection should be keeped alive */
          reset_client_buffer_state(clientfd);
        } else if (http_res == 0) {   /* Connection should be closed */
          clear_client_by_idx(clientfd, i);
        } else if (http_res == -1) {  /* Internal error in http handler */
          clear_client_by_idx(clientfd, i);
        }
        //clr_fl(clientfd, O_NONBLOCK);   /* clear nonblocking */
      } else if (nbytes <= 0) {
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
    } else if (FD_ISSET(clientfd, &p->write_fds)){
      /* Handle clients that are ready to be response to */
      set_fl(clientfd, O_NONBLOCK);     /* set nonblocking */
      dynamic_buffer *client_buffer = p->client_buffer[i];
      size_t send_granularity = 8192;
      size_t send_len = MIN(send_granularity, client_buffer->offset-client_buffer->send_offset);
      Sendn(clientfd, client_buffer->buffer, send_len);
      client_buffer->send_offset += send_len;
    } else {
      /* Can not handle this type of event */
      dump_log("[handle_client handle_client()] Can not deal with this type of event");
      clear_client(clientfd);
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

  /* free client buffer */
  free_dbuffer(p->client_buffer[idx]);
  p->received_header[idx] = 0;
}

/*
 * Clear resource associated with client
 */
void clear_client(int client) {
  int idx, cliendfd;

  for (idx = 0; idx < FD_SETSIZE; idx++) {
    cliendfd = p->client_fd[idx];
    if (cliendfd == client) {
      /* Free resources and reset all states */
      close(cliendfd);
      FD_CLR(cliendfd, &p->master);
      p->client_fd[idx] = -1;

      /* free client buffer */
      free_dbuffer(p->client_buffer[idx]);

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
 * Print out client pool's current state.
 * For debug usage.
 */
void print_pool() {
  int i;
  console_log("***********************Current Pool State************************");
  for (i = 0; i <= p->maxi; i++) {
    console_log("<Position %d>", i);
    console_log("---> Clientfd: %d", p->client_fd[i]);
    console_log("---> Client Buffer Cap: %d", p->client_buffer[i]->capacity);
    console_log("---> Client Buffer Offset: %d", p->client_buffer[i]->offset);
    console_log("---> Received Header Yet? : %d", (int)p->received_header[i]);
  }
  console_log("*****************************************************************");
}

/*
 * Append received content to client's buffer
 * Return appended buffer so far.
 * Deprecated.
 */
//char* append_request(int client, client_pool *p, char *buf, ssize_t len) {
//  int i,clientfd;
//  int expansion;
//
//  for (i = 0; (i <= p->maxi) && p->nready > 0; i++) {
//    clientfd = p->client_fd[i];
//    if (clientfd == client) {
//      size_t offset = p->buffer_offset[i];
//      size_t current_capacity = p->buffer_cap[i];
//
////#ifdef DEBUG_VERBOSE
////      console_log("[INFO] Trying to append request to client buffer...");
////      console_log("[INFO] Client %d buffer capacity %d", clientfd, current_capacity);
////      console_log("[INFO] Client %d need buffer space %d (%d offset + %d nbytes)",
////                  clientfd, (offset+len), offset, len);
////#endif
//
//      /* dynamically allocate space */
//      if ((offset+len) > current_capacity) {
//        /* Determine expansion:  keep doubling current capacity until the expansion size
//           is larger than the size of needed space, which is (offset+len). */
//        expansion = current_capacity;
//        while ((offset+len) > expansion)  expansion *= 2;
//
//        p->client_buffer[i] = realloc(p->client_buffer[i], expansion);
//        if (p->client_buffer[i] == NULL) {
//          err_sys("Memory can not be allocated");
//        }
//        p->buffer_cap[i] = expansion;
////#ifdef DEBUG_VERBOSE
////        console_log("[INFO] Client %d buffer capacity expands from %d bytes to %d bytes",
////                    clientfd, current_capacity, expansion);
////#endif
//      }
//
//      strncpy(p->client_buffer[i]+offset, buf, len);
//      p->buffer_offset[i] += len;
//      return p->client_buffer[i];
//    }
//  }
//  // no such client to be appended
//#ifdef DEBUG_VERBOSE
//  console_log("[INFO] Client not found when trying to append request content");
//#endif
//  dump_log("[INFO] Client not found when trying to append request content");
//  return NULL;
//}

/*
 * Get client buffer's offset, which is current length of request content.
 */
size_t get_client_buffer_offset(int client) {
  int i;
  int clientfd;
  for (i = 0; (i <= p->maxi) && p->nready > 0; i++) {
    clientfd = p->client_fd[i];
    if (clientfd == client) return p->client_buffer[i]->offset;
  }
  // no such client to be found
#ifdef DEBUG_VERBOSE
  console_log("[INFO] Client not found when trying to get buffer offset");
#endif
  dump_log("[INFO] Client not found when trying to get buffer offset");
  return 0;
}

/*
 * Set a client's state from <header not received> to <header received>
 */
void set_header_received(int client, size_t offset) {
  int i, clientfd;

  for (i = 0; i < FD_SETSIZE; i++) {
    clientfd = p->client_fd[i];
    if (clientfd == client) {
      p->received_header[i] = offset;
      return ;
    }
  }
  // no such client to be found
#ifdef DEBUG_VERBOSE
  console_log("[INFO] Client not found when setting received header to be true");
#endif
  dump_log("[INFO] Client not found when setting received header to be true");
}

/*
 * Used for pipelined request. If connection should be kept alive, upon finish
 * the response, reset client's state to wait for next HTTP request.
 */
void reset_client_buffer_state(int client) {
  int i, clientfd;

  for (i = 0; i < FD_SETSIZE; i++) {
    clientfd = p->client_fd[i];
    if (clientfd == client) {
      reset_dbuffer(p->client_buffer[i]);
      p->received_header[i] = 0;
      return ;
    }
  }
#ifdef DEBUG_VERBOSE
  console_log("[INFO] Client not found when resetting client buffer state");
#endif
  dump_log("[INFO] Client not found when resetting client buffer state");
}

/*
 * Return 1 if header is totally received.
 * Return 0 if not.
 * Return -1 if header is larger than 8192. Should indicate error.
 */
size_t handle_recv_header(int clientfd, char *client_buffer) {
  char *header_end_id = "\r\n\r\n";
  char *header_end = strstr(client_buffer, header_end_id);
  size_t header_len = get_client_buffer_offset(clientfd);
  if (header_end == NULL){
    if (header_len < BUF_SIZE) return 0;
    else return -1;
  } else {
    /* if header is received, this indicates the starting point of the message body. */
    return (header_end-client_buffer)+4;
  }
}