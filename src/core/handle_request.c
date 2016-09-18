//
// Created by XiaotongSun on 16/9/17.
//

#include "handle_request.h"


/* Defineds tokens */
const char *clrf = "\r\n";
const char *sp = " ";
const char *http_version = "HTTP/1.1";


/*
 * Return 0 on normal case (2xx status code family).
 * Return corresponding status code on inormal case (4xx, 5xx status code family).
 * Return -1 on internal error
 */
int handle_http_request(int clientfd, char *buf, ssize_t len){
#ifdef DEBUG_VERBOSE
  console_log("Receiving request : %s\nLength: %d", buf, strlen(buf));
#endif

  /* reply buffer */
  char reply[BUF_SIZE];
  memset(reply, 0, sizeof(char)*BUF_SIZE);

  /* parse header */
  Request *request = parse(buf, len);
  if (request == NULL) {
    /* if parsing fails, send back 400 error */
    send_error(reply, (char*)"400", (char*)"Bad Request");
    reply_to_client(clientfd, reply);
    return 400;
  }

  /* check version */
  int correct_version;
  correct_version = check_http_version(request);
  if (correct_version < 0) {
    /* if checking version fails, send back 505 error */
    send_error(reply, (char*)"505", (char*)"HTTP Version not supported");
    reply_to_client(clientfd, reply);
    return 505;
  }

  char *method = request->http_method;

  if (strcmp(method, "GET"))
  {
    send_msg(reply, "to deal with GET");
    reply_to_client(clientfd, reply);
  }
  else if (strcmp(method, "HEAD"))
  {
    send_msg(reply, "to deal with GET");
    reply_to_client(clientfd, reply);
  }
  else if (strcmp(method, "POST"))
  {
    send_msg(reply, "to deal with GET");
    reply_to_client(clientfd, reply);
  }
  else
  {
    send_error(reply, (char*)"501", (char*)"Not Implemented");
    reply_to_client(clientfd, reply);
    return 501;
  }

  return 0;
}

/*
 * Reply to client
 */
void reply_to_client(int client, char *reply) {
  Sendn(client, reply, strlen(reply));
}

/*
 * Check http version in header
 * Return 0 on correct version, -1 on incorrect one
 */
int check_http_version(Request *req){
  char *version = req->http_version;
  char *target = "HTTP/1.1\0";
  if (strcmp(version, target)) {
#ifdef DEBUG_VERBOSE
    console_log("request version: %s", req->http_version);
    console_log("target version: %s", target);
    console_log("Version not match!");
#endif
    return -1;
  }
  return 0;
}

/* For develop convenience, send random msg to client */
int send_msg(char *reply, char* msg) {
  sprintf(reply, "%s\n", msg);
  return 0;
}

/*
 * Send 4xx/5xx family reply
 */
int send_error(char *reply, char *code, char*reason) {
  sprintf(reply, "%s%s%s%s%s%s", http_version, sp, code, sp, reason, clrf);
  return 0;
}


///* Status line helpers */
//int send_status_line(char *reply, const char *code, char *reason) {
//  const int status_line_size = 256;
//  char status_line_buf[status_line_size];
//
//  memset(status_line_buf, 0, sizeof(char)*status_line_size);
//  sprintf(status_line_buf, "%s%s%s%s%s%s", http_version, sp, code, sp, reason, clrf);
//
//  size_t status_line_len = strlen(status_line_buf);
//  size_t reply_buf_len = strlen(reply);
//  if ((reply_buf_len+status_line_len) > BUF_SIZE) {
//    console_log("buf overflow");
//  }
//  char *start = reply + reply_buf_len;
//  strncpy(start, status_line_buf, status_line_len);
//  return 0;
//}
