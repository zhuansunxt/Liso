#include "handle_clients.h"

/* Global client pool */
client_pool *p;

/* Global sockets server listen to*/
int listenfd;       /* HTTP accept socket */
int ssl_socket;     /* HTTPS accept socket */

/* Global ports */
int port;
int https_port;

/* Initialize client pool */
void init_pool() {
  p = (client_pool*)malloc(sizeof(client_pool));

  /* Init pool's global data */
  FD_ZERO(&p->master);
  FD_ZERO(&p->read_fds);
  FD_ZERO(&p->write_fds);
  FD_SET(listenfd, &p->master);
  FD_SET(ssl_socket, &p->master);
  p->maxfd = MAX(listenfd, ssl_socket);
  p->nready = 0;
  p->maxi = 0;

  /* Init client's specifc data */
  int i;
  for (i = 0; i < FD_SETSIZE; i++) {
    p->client_fd[i] = -1;     /* - 1 indicates avaialble entry */
    p->client_buffer[i] = NULL;
    p->back_up_buffer[i] = NULL;
    p->received_header[i] = 0;
    p->should_be_close[i] = 0;
    p->state[i] = INVALID;
    p->remote_addr[i] = NULL;

    /* SSL */
    p->type[i] = INVALID_CLIENT;
    p->context[i] = NULL;

    /* CGI */
    p->cgi_client[i] = -1;
  }
}

void add_client_to_pool(int newfd, SSL* client_context, client_type ctype, char *remoteaddr) {
  int i;
  p->nready--;    /* listener socket */

  for (i = 0; i < FD_SETSIZE; i++) {
    if (p->client_fd[i] < 0) {  /* Find a available entry */

      /* Update global data */
      FD_SET(newfd, &p->master);
      p->maxfd = MAX(newfd, p->maxfd);
      p->maxi = MAX(i, p->maxi);

      /* Initialize client specific data */
      p->client_fd[i] = newfd;
      p->client_buffer[i] = (dynamic_buffer*)malloc(sizeof(dynamic_buffer));
      init_dbuffer(p->client_buffer[i]);
      p->received_header[i] = 0;
      p->should_be_close[i] = 0;
      p->state[i] = READY_FOR_READ;
      p->remote_addr[i] = remoteaddr;

      /* SSL */
      p->type[i] = ctype;
      if (p->type[i] == HTTPS_CLIENT) p->context[i] = client_context;

      /* CGI */
      p->cgi_client[i] = -1;
      break;
    }
  }

  if (i == FD_SETSIZE) {
    /* Coundn't find available slot in pool */
    err_sys("No available slot for new client connection");
  }
}

void add_cgi_fd_to_pool(int clientfd, int cgi_fd, client_state state) {
  int i;

  for (i = 0; i < FD_SETSIZE; i++) {
    if (p->client_fd[i] < 0) {
      /* Update global data */
      FD_SET(cgi_fd, &p->master);
      p->maxfd = MAX(cgi_fd, p->maxfd);
      p->maxi = MAX(i, p->maxi);

      /* Update client data */
      p->client_fd[i] = cgi_fd;
      p->state[i] = state;

      /* CGI */
      p->cgi_client[i] = clientfd;
      break;
    }
  }
  if (i == FD_SETSIZE) {
    /* Coundn't find available slot in pool */
    err_sys("No available slot for new cgi process fd");
  }
}

void clear_cgi_from_pool(int clientfd) {
  int i;
  for (i = 0; i < FD_SETSIZE; i++) {
    if (p->cgi_client[i] == clientfd) {
      /* Update global data */
      FD_CLR(p->client_fd[i], &p->master);

      /* Update client data */
      close(p->client_fd[i]);
      p->client_fd[i] = -1;
      if (p->client_buffer[i] != NULL) free_dbuffer(p->client_buffer[i]);
      if (p->back_up_buffer[i] != NULL) free_dbuffer(p->back_up_buffer[i]);
      p->client_buffer[i] = NULL;
      p->back_up_buffer[i] = NULL;
      p->received_header[i] = 0;
      p->should_be_close[i] = 0;
      p->state[i] = INVALID;
      p->remote_addr[i] = NULL;

      /* SSL */
      if (p->type[i] == HTTPS_CLIENT && p->context[i] != NULL) SSL_free(p->context[i]);
      p->type[i] = INVALID_CLIENT;
      p->context[i] = NULL;

      /* CGI */
      p->cgi_client[i] = -1;
      break;
    }
  }

  if (i == FD_SETSIZE) {
    /* Coundn't find available slot in pool */
#ifdef DEBUG_VERBOSE
    console_log("[clear cgi from pool] No cgi info of client %d", clientfd);
#endif
    dump_log("[clear cgi from pool] No cgi info of client %d", clientfd);
  }
}

void clear_client_by_idx(int client_fd, int idx){
  /* Update global data */
  FD_CLR(client_fd, &p->master);

  /* Update client data */
  close(client_fd);
  p->client_fd[idx] = -1;
  if (p->client_buffer[idx] != NULL) free_dbuffer(p->client_buffer[idx]);
  if (p->back_up_buffer[idx] != NULL) free_dbuffer(p->back_up_buffer[idx]);
  p->client_buffer[idx] = NULL;
  p->back_up_buffer[idx] = NULL;
  p->received_header[idx] = 0;
  p->should_be_close[idx] = 0;
  p->state[idx] = INVALID;
  p->remote_addr[idx] = NULL;

  /* SSL */
  if (p->type[idx] == HTTPS_CLIENT && p->context[idx] != NULL) SSL_free(p->context[idx]);
  p->type[idx] = INVALID_CLIENT;
  p->context[idx] = NULL;

  /* CGI */
  p->cgi_client[idx] = -1;
}

void clear_client(int client) {
  int idx, cliendfd;

  for (idx = 0; idx < FD_SETSIZE; idx++) {
    cliendfd = p->client_fd[idx];
    if (cliendfd == client) {
      clear_client_by_idx(client, idx);
      break;
    }
  }
#ifdef DEBUG_VERBOSE
  console_log("[clear client] client not found when clearing client %d", client);
#endif
  dump_log("[clear client] Client not found when clearing client %d", client);
}

void clear_pool() {
  int i, clientfd;
  for (i = 0; i < FD_SETSIZE; i++) {
    clientfd = p->client_fd[i];
    if (clientfd > 0) {
      clear_client_by_idx(clientfd, i);
    }
  }
  free(p);
}

void handle_clients() {
  ssize_t  nbytes;
  int i, clientfd;
  char buf[BUF_SIZE];

  /* Scan client pool, check read & write events */
  for (i = 0; (i <= p->maxi) ; i++) {
    clientfd = p->client_fd[i];
    if (clientfd <= 0) continue;

    memset(buf, 0, sizeof(char)*BUF_SIZE);
    set_fl(clientfd, O_NONBLOCK);     /* set fd to be non-blocking */

    if ((FD_ISSET(clientfd, &p->read_fds) || p->back_up_buffer[i] != NULL) && (p->state[i] == READY_FOR_READ)) {
      console_log("[Main select loop] Client %d is ready for reading", clientfd);
      /* Check whether this client has historical pending request data */
      if (p->back_up_buffer[i] != NULL) {
        append_content_dbuffer(p->client_buffer[i], p->back_up_buffer[i]->buffer, p->back_up_buffer[i]->offset);
        free_dbuffer(p->back_up_buffer[i]);
        p->back_up_buffer[i] = NULL;
      }

      /* If there's upcoming bytes, append data to client's buffer */
      if (FD_ISSET(clientfd, &p->read_fds)) {
        p->nready--;
        if (p->type[i] == HTTP_CLIENT) {
          nbytes = recv(clientfd, buf, BUF_SIZE, 0);
        } else {
          nbytes = SSL_read(p->context[i], buf, BUF_SIZE);
        }

        if (nbytes <= 0) {
          if (nbytes == 0) {    /* Connection closed by client */
            console_log("[INFO] Connection closed by client %d", clientfd);
          } else {
            if (errno == EINTR) continue;
            console_log("[INFO] Error on recv() from client %d", clientfd);
          }
          clear_client_by_idx(clientfd, i);
          continue;
        }

        append_content_dbuffer(p->client_buffer[i], buf, nbytes);
      }

      /* Check whether received request content has complete HTTP header */
      size_t header_recv_offset = handle_recv_header(clientfd, p->client_buffer[i]->buffer);
      if (header_recv_offset == 0) {
        continue;
      } else if (header_recv_offset == -1) {
        dump_log("[EXCEPTION] Client %d header length is larger than buffer size. Abandoned it!", clientfd);
        clear_client_by_idx(clientfd, i);
        continue;
      }

      set_header_received(clientfd, header_recv_offset);

      host_and_port has;
      has.host = p->remote_addr[i];
      has.port = (p->type[i] == HTTP_CLIENT) ? port : https_port;

      dynamic_buffer * pending_request = (dynamic_buffer*)malloc(sizeof(dynamic_buffer));
      init_dbuffer(pending_request);

      /* Handle HTTP request */
      http_process_result result = handle_http_request(clientfd, p->client_buffer[i], header_recv_offset, has, pending_request);

      if (pending_request->offset == 0) {
        free_dbuffer(pending_request);
        p->back_up_buffer[i] = NULL;
      } else {
        p->back_up_buffer[i] = pending_request;
      }

      CGI_executor *executor;

      switch (result){
        case ERROR:
          /* Internal error happens, reply 500 error, close connection */
          reply_500(p->client_buffer[i]);
        case CLOSE:
          /* Connection should be closed, close connection */
          p->should_be_close[i] = 1;
        case PERSIST:
          /* Normal result, keep connection */
          p->state[i] = READY_FOR_WRITE;
          break;
        case NOT_ENOUGH_DATA:
          /* For POST request, data is not fully received according to Content-Length header */
          continue;
        case CGI_READY_FOR_WRITE_CLOSE:
          /* CGI request, has body to write to CGI process, and should close connection*/
          p->should_be_close[i] = 1;
        case CGI_READY_FOR_WRITE:
          /* CGI request, has body to write to CGI process */
          reset_dbuffer(p->client_buffer[i]);
          p->state[i] = WAITING_FOR_CGI;
          executor = get_CGI_executor_by_client(clientfd);
          if (executor == NULL){
#ifdef DEBUG_VERBOSE
            console_log("[ERROR] Can not get CGI executor to write to on client %d", clientfd);
#endif
            dump_log("[ERROR] Can not get CGI executor to write to on client %d", clientfd);
          }
          add_cgi_fd_to_pool(clientfd, executor->stdin_pipe[1], CGI_FOR_WRITE);
          break;
        case CGI_READY_FOR_READ_CLOSE:
          /* CGI request, has content to read from CGI process, and should close connection*/
          p->should_be_close[i] = 1;
        case CGI_READY_FOR_READ:
          /* CGI request, has content to read from CGI process */
          console_log("Client %d handle result: CGI_READY_FOR_READ");
          reset_dbuffer(p->client_buffer[i]);
          p->state[i] = WAITING_FOR_CGI;
          print_CGI_pool();
          executor = get_CGI_executor_by_client(clientfd);
          if (executor == NULL) {
#ifdef DEBUG_VERBOSE
            console_log("[ERROR] Can not get CGI executor to read from on client %d", clientfd);
#endif
            dump_log("[ERROR] Can not get CGI executor to read from on client %d", clientfd);
          }
          add_cgi_fd_to_pool(clientfd, executor->stdout_pipe[0], CGI_FOR_READ);
          print_pool();
          break;
        default:
          break;
      }
    }

    if (FD_ISSET(clientfd, &p->write_fds) && p->state[i] == READY_FOR_WRITE) {
      /* Handle clients that are ready to be response to */
      console_log("[Main select loop] Client %d is ready for writing", clientfd);
      dynamic_buffer *client_buffer = p->client_buffer[i];
      size_t send_granularity = BUF_SIZE;
      size_t send_len = MIN(send_granularity, client_buffer->offset - client_buffer->send_offset);
      size_t sent_bytes;

      print_dbuffer(client_buffer);

      if (p->type[i] == HTTP_CLIENT)
        sent_bytes = send(clientfd, client_buffer->buffer + client_buffer->send_offset, send_len, 0);
      else
        sent_bytes = SSL_write(p->context[i], client_buffer->buffer + client_buffer->send_offset, send_len);
      client_buffer->send_offset += sent_bytes;

      if (client_buffer->send_offset >= client_buffer->offset) {
        reset_client_buffer_state_by_idx(clientfd, i);
        p->received_header[i] = 0;
        p->state[i] = READY_FOR_READ;
        p->remote_addr[i] = NULL;
        if (p->should_be_close[i] == 1) clear_client_by_idx(clientfd, i);
        console_log("End handling a full request for client %d", clientfd);
        print_pool();
        //print_CGI_pool();
      }
      p->nready--;
    }

    if (FD_ISSET(clientfd, &p->write_fds) && p->state[i] == CGI_FOR_WRITE) {
      console_log("[CGI port] Client %d 's CGI ready for being written to!", clientfd);
      CGI_executor *executor = get_CGI_executor_by_client(p->cgi_client[i]);
      if (executor == NULL) {
#ifdef DEBUG_VERBOSE
        console_log("[ERROR] Can not get CGI executor to write to on client %d", p->cgi_client[i]);
#endif
        dump_log("[ERROR] Can not get CGI executor to read from on client %d", p->cgi_client[i]);
      }
      dynamic_buffer *cgi_write_buffer = executor->cgi_buffer;

      size_t write_granularity = BUF_SIZE;
      size_t write_len = MIN(write_granularity, cgi_write_buffer->offset - cgi_write_buffer->send_offset);
      size_t sent_bytes = write(executor->stdin_pipe[1],
                                cgi_write_buffer->buffer+cgi_write_buffer->send_offset, write_len);
      cgi_write_buffer->send_offset += sent_bytes;

      if (cgi_write_buffer->send_offset >= cgi_write_buffer->offset) {
        reset_dbuffer(executor->cgi_buffer);
        p->state[i] = CGI_FOR_READ;
        //print_pool();
        int client_socket = p->cgi_client[i];
        clear_cgi_from_pool(p->cgi_client[i]);
        console_log("after removing CGI write fd from pool");
        console_log("adding CGI read fd to pool");
        add_cgi_fd_to_pool(client_socket, executor->stdout_pipe[0], CGI_FOR_READ);
        //print_pool();
      }
      p->nready--;
    }

    if (FD_ISSET(clientfd, &p->read_fds) && p->state[i] == CGI_FOR_READ) {
      console_log("[CGI port] Client %d 's CGI ready for being read from!", clientfd);
      print_pool();
      CGI_executor *executor = get_CGI_executor_by_client(p->cgi_client[i]);
      if (executor == NULL) {
#ifdef DEBUG_VERBOSE
        console_log("[ERROR] Can not get CGI executor to read from on client %d", p->cgi_client[i]);
#endif
        dump_log("[ERROR] Can not get CGI executor to read from on client %d", p->cgi_client[i]);
      }
      char cgi_buff[BUF_SIZE];
      int read_ret;
      dynamic_buffer *client_buffer = get_client_buffer_by_client(p->cgi_client[i]);
      print_dbuffer(client_buffer);
      if ((read_ret = read(clientfd, cgi_buff, BUF_SIZE)) > 0) {
        append_content_dbuffer(client_buffer, cgi_buff, read_ret);
      }

      if (read_ret == 0) {
        console_log("Finish reading from CGI port %d on client %d", clientfd, executor->clientfd);
        console_log("What is read : %s", client_buffer->buffer);
        p->state[get_client_index(p->cgi_client[i])] = READY_FOR_WRITE;
        /* Clear CGI related resources */
        clear_cgi_from_pool(executor->clientfd);
        clear_CGI_executor_by_client(executor->clientfd);
        console_log("Clearing resources...");
        print_pool();
        print_CGI_pool();
      }
      p->nready--;
    }

  } // End for loop.
} // End function.

void print_pool() {
  int i;
  console_log("***********************Current Pool State************************");
  console_log("** maxfd: %d", p->maxfd);
  console_log("** maxi: %d", p->maxi);
  for (i = 0; i <= p->maxi; i++) {
    console_log("-");
    console_log("<Position %d>", i);
    console_log("---> Clientfd: %d", p->client_fd[i]);
    console_log("---> Client Buffer Info");
    print_dbuffer(p->client_buffer[i]);
    console_log("---> Client Backup Buffer Info");
    print_dbuffer(p->back_up_buffer[i]);
    console_log("---> Received Header Yet? : %s", ((int)p->received_header[i] == 0) ? "No" : "Yes");
    console_log("---> Received Should Be Closed Yet? : %s", ((int)p->should_be_close[i] == 0) ? "No" : "Yes");
    console_log("---> Remote Addr: %s", (p->remote_addr[i] == NULL) ? "None" : p->remote_addr[i]);
    switch (p->state[i]) {
      case INVALID:
        console_log("---> Client State: INVALID");
        break;
      case READY_FOR_READ:
        console_log("---> Client State: READY_FOR_READ");
        break;
      case READY_FOR_WRITE:
        console_log("---> Client State: READY_FOR_WRITE");
        break;
      case CGI_FOR_READ:
        console_log("---> Client State: CGI_FOR_READ");
        console_log("---> Client Sockets: %d", p->cgi_client[i]);
        break;
      case CGI_FOR_WRITE:
        console_log("---> Client State: CGI_FOR_WRITE");
        console_log("---> Client Sockets: %d", p->cgi_client[i]);
        break;
      case WAITING_FOR_CGI:
        console_log("---> Client State: WAITING_FOR_CGI");
        break;
      default:
        console_log("---> Client State: UNKNOWN STATE");
        break;
    }

    switch (p->type[i]) {
      case HTTP_CLIENT:
        console_log("---> Client Type: HTTP");
        break;
      case HTTPS_CLIENT:
        console_log("---> Client Type: HTTPS");
        break;
      case INVALID_CLIENT:
        console_log("---> Client Type: INVALID");
        break;
      default:
        console_log("---> Client State: UNKNOWN TYPE");
        break;
    }
  }
  console_log("*******************End Current Pool State********************");
}

/*
 * Given client socket, return client index in pool.
 * Do not use for CGI fd as argument.
 * */
int get_client_index(int client) {
  int idx = 0;
  for (; idx <= p->maxi; idx++) {
    if (p->client_fd[idx] == client) {
      return idx;
    }
  }
  return -1;
}

/*
 * Get client buffer's offset, which is current length of request content.
 * Return -1 when not found.
 * Do not use for CGI fd as argument.
 */
size_t get_client_buffer_offset(int client) {
  int i;
  int clientfd;
  for (i = 0; (i <= p->maxi); i++) {
    clientfd = p->client_fd[i];
    if (clientfd == client && p->client_buffer[i] != NULL) return p->client_buffer[i]->offset;
  }
  // no such client to be found
#ifdef DEBUG_VERBOSE
  console_log("[get client buffer offset] Client not found when trying to get buffer offset");
#endif
  dump_log("[get client buffer offset] Client not found when trying to get buffer offset");
  return -1;
}

/*
 * Do not use for CGI fd as argument.
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
  console_log("[set header received] Client not found when setting received header to be true");
#endif
  dump_log("[set header received] Client not found when setting received header to be true");
}

void reset_client_buffer_state_by_idx(int client, int idx) {
  if (p->client_buffer[idx] != NULL) {
    reset_dbuffer(p->client_buffer[idx]);
  } else {
#ifdef DEBUG_VERBOSE
    console_log("[reset client buffer] client %d buffer is NULL", client);
#endif
    dump_log("[reset client buffer] client %d buffer is NULL", client);
  }
}

dynamic_buffer* get_client_buffer_by_client(int client) {
  int idx = 0;
  for (; idx < FD_SETSIZE; idx++) {
    if (p->client_fd[idx] == client && p->client_buffer[idx] != NULL) {
      return p->client_buffer[idx];
    }
  }

#ifdef DEBUG_VERBOSE
  console_log("[get client buffer by idx] client %d buffer not found", client);
#endif
  dump_log("[get client buffer by idx] client %d buffer not found", client);
  return NULL;
}

/*
 * Return header's length.
 * Return 0 if not header is not fully received yet.
 * Return -1 if header is larger than 8192. Should indicate error.
 */
size_t handle_recv_header(int clientfd, char *client_buffer) {
  char *header_end_id = "\r\n\r\n";
  char *header_end = strstr(client_buffer, header_end_id);
  size_t header_len = get_client_buffer_offset(clientfd);
  if (header_end == NULL) {
    if (header_len < BUF_SIZE) return 0;
    else return -1;
  } else {
    /* if header is received, this indicates the starting point of the message body. */
    return (header_end - client_buffer) + strlen(header_end_id);
  }
}

