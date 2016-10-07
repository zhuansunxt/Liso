#include "handle_clients.h"

/* Global client pool */
client_pool pool;
client_pool *p = &pool;

/* Global socket */
int listenfd;
int ssl_socket;

/*
 * Initialize client pool with <listenfd> as the only active socket descriptor.
 * After init, client pool is empty with only one listener socket and is ready
 * for accepting client requests.
 */
void init_pool() {
  FD_ZERO(&p->master);
  FD_ZERO(&p->read_fds);
  FD_SET(listenfd, &p->master);
  FD_SET(ssl_socket, &p->master);

  console_log("[Init Pool] http socket : %d, https socket: %d", listenfd, ssl_socket);
  p->maxfd = MAX(listenfd, ssl_socket);
  p->nready = 0;
  p->maxi = -1;
  int i;
  for (i = 0; i < FD_SETSIZE; i++) {
    p->client_fd[i] = -1;     /* - 1 indicates avaialble entry */
    p->client_buffer[i] = NULL;
    p->received_header[i] = 0;
    p->should_be_close[i] = 0;
    p->context[i] = NULL;
  }
}

/*
 * Add new client socket descriptor to client pool.
 */
void add_client_to_pool(int newfd, SSL* client_context, client_type ctype) {
  int i;
  p->nready--;    /* listener socket */

  for (i = 0; i < FD_SETSIZE; i++) {
    if (p->client_fd[i] < 0) {
      /* Find a available entry */
      p->client_fd[i] = newfd;

      /* Initialize client buffer */
      p->client_buffer[i] = (dynamic_buffer*)malloc(sizeof(dynamic_buffer));
      init_dbuffer(p->client_buffer[i]);

      p->state[i] = READY_FOR_READ;
      p->type[i] = ctype;
      if (p->type[i] == HTTPS_CLIENT) p->context[i] = client_context;
      p->received_header[i] = 0;
      p->should_be_close[i] = 0;
      FD_SET(newfd, &p->master);
      p->maxfd = MAX(newfd, p->maxfd);
      p->maxi = MAX(i, p->maxi);
      break;
    }
  }

  if (i == FD_SETSIZE) {
    /* Coundn't find available slot in pool */
#ifdef DEBUG_VERBOSE
    console_log("[Client pool] Too many clients! No available slot for new client connection");
#endif
    dump_log("[ERROR] Too many clients! No available slot for new client connection");
  }
}

void handle_clients() {
  ssize_t  nbytes;
  int i, clientfd;
  char buf[BUF_SIZE];

  for (i = 0; (i <= p->maxi) && (p->nready > 0) ; i++) {
    clientfd = p->client_fd[i];
    if (clientfd <= 0) continue;

    memset(buf, 0, sizeof(char)*BUF_SIZE);
    set_fl(clientfd, O_NONBLOCK);     /* set fd to be non-blocking */

    if (FD_ISSET(clientfd, &p->read_fds) && p->state[i] == READY_FOR_READ) {
      if (p->type[i] == HTTP_CLIENT)
        nbytes = recv(clientfd, buf, BUF_SIZE, 0);
      else
        nbytes = SSL_read(p->context[i], buf, BUF_SIZE);

      if (nbytes > 0) {
        append_content_dbuffer(p->client_buffer[i], buf, nbytes);

        /* Check whether received request content has complete HTTP header.
         * If <clrfclrf> is detected, *or* current header_len is larger than
         * BUF_SIZE(8192), we send received accumulated buffer to HTTP handler.*/

        size_t header_recv_offset = handle_recv_header(clientfd, p->client_buffer[i]->buffer);
        if (header_recv_offset == 0) {
          continue;
        } else if (header_recv_offset == -1) {
          dump_log("[EXCEPTION] Client %d header length is larger than buffer size. Abandoned it!", clientfd);
          clear_client_by_idx(clientfd, i);
          continue;
        }

        set_header_received(clientfd, header_recv_offset);

        http_process_result result = handle_http_request(clientfd, p->client_buffer[i], header_recv_offset);
        switch (result){
          case ERROR:
            reply_500(p->client_buffer[i]);
          case CLOSE:
            console_log("Client %d http processing result: CLOSE");
            p->should_be_close[i] = 1;
          case PERSIST:
            p->state[i] = READY_FOR_WRITE;
            break;
          case NOT_ENOUGH_DATA:
            continue;                           /* Upon not-enough-data result: keep state same and just move on */
          default:
            break;
        }
      } else if (nbytes <= 0) {
        if (nbytes == 0) {    /* Connection closed by client */
          dump_log("[INFO] Connection closed by client %d", clientfd);
        } else {
          if (errno == EINTR) continue;
          dump_log("[INFO] Error on recv() from client %d", clientfd);
        }
        clear_client_by_idx(clientfd, i);
      }
      p->nready--;
    }

    if (FD_ISSET(clientfd, &p->write_fds) && p->state[i] == READY_FOR_WRITE) {
      /* Handle clients that are ready to be response to */
      dynamic_buffer *client_buffer = p->client_buffer[i];
      size_t send_granularity = BUF_SIZE;
      size_t send_len = MIN(send_granularity, client_buffer->offset - client_buffer->send_offset);
      size_t sent_bytes;
      if (p->type[i] == HTTP_CLIENT)
        sent_bytes = send(clientfd, client_buffer->buffer + client_buffer->send_offset, send_len, 0);
      else
        sent_bytes = SSL_write(p->context[i], client_buffer->buffer + client_buffer->send_offset, send_len);
      client_buffer->send_offset += sent_bytes;

#ifdef DEBUG_VERBOSE
      console_log("[Client Pool] Successfully sent %d bytes to client %d", send_len, clientfd);
#endif

      if (client_buffer->send_offset >= client_buffer->offset) {
        reset_client_buffer_state_by_idx(clientfd, i);
        if (p->should_be_close[i] == 1) clear_client_by_idx(clientfd, i);
      }
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
  free_dbuffer(p->client_buffer[idx]);
  if (p->type[idx] == HTTPS_CLIENT) SSL_free(p->context[idx]);
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
      free_dbuffer(p->client_buffer[idx]);
      if (p->type[idx] == HTTPS_CLIENT) SSL_free(p->context[idx]);
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
    console_log("---> Client State: %s", (p->state == READY_FOR_READ) ? "READY_FOR_READ" : "READY_FOR_WRITE");
    console_log("---> Client Type: %s", (p->type == HTTP_CLIENT) ? "HTTP" : "HTTPS");
    console_log("---> Received Header Yet? : %s", ((int)p->received_header[i] == 0) ? "No" : "Yes");
  }
  console_log("*****************************************************************");
}

/*
 * Get client buffer's offset, which is current length of request content.
 */
size_t get_client_buffer_offset(int client) {
  int i;
  int clientfd;
  for (i = 0; (i <= p->maxi); i++) {
    clientfd = p->client_fd[i];
    if (clientfd == client) return p->client_buffer[i]->offset;
  }
  // no such client to be found
#ifdef DEBUG_VERBOSE
  console_log("[INFO] Client not found when trying to get buffer offset");
#endif
  dump_log("[INFO] Client not found when trying to get buffer offset");
  return -1;
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
void reset_client_buffer_state_by_idx(int client, int idx) {
  reset_dbuffer(p->client_buffer[idx]);
  p->received_header[idx] = 0;
  p->state[idx] = READY_FOR_READ;
  return ;
}

/*
 * Return header's length.
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
    return (header_end-client_buffer)+strlen(header_end_id);
  }
}