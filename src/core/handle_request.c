//
// Created by XiaotongSun on 16/9/17.
//

#include "handle_request.h"

char *WWW_FOLDER;

/* Defineds tokens */
const char *clrf = "\r\n";
const char *sp = " ";
const char *http_version = "HTTP/1.1";
const char *colon = ":";

/* Constants string values */
const char *default_index_file = "index.html";
const char *server_str = "Liso/1.0";


/*
 * Return 0 when the current connection should not be closed.
 * Return corresponding status code on inormal case (4xx, 5xx status code family).
 * Return -1 on internal error.
 * The caller of this function should check the return value:
 *   - If it's larger than 0, it means that server already gave a response to the client.
 *     The caller then should release all resources associated with that client.
 *   - If it's equal to 0, it means that http_handler expects more info from this current
 *     request, e.g. for POST method, only sees header but message body is not fully
 *     received.
 *   - If it's less than 0, the caller should handle the error condition.
 */
int handle_http_request(int clientfd, char *buf, ssize_t len){
#ifdef DEBUG_VERBOSE
  console_log("[INFO][HTTP] Client %d Receiving request : %s\nLength: %d", clientfd, buf, strlen(buf));
#endif

  /* reply buffer */
  char reply[BUF_SIZE];
  memset(reply, 0, sizeof(char)*BUF_SIZE);

  /* parse header */
  Request *request = parse(buf, len);

  /* Handle 400 Error: Bad request */
  if (request == NULL) {
    /* if parsing fails, send back 400 error */
    send_response(reply, (char*)"400", (char*)"Bad Request");
    reply_to_client(clientfd, reply);
    free_request(request);
    return 400;
  }

  /* Handle 505 Error: Version not supported */
  int correct_version;
  correct_version = check_http_version(request);
  if (correct_version < 0) {
    /* if checking version fails, send back 505 error */
    send_response(reply, (char*)"505", (char*)"HTTP Version not supported");
    reply_to_client(clientfd, reply);
    free_request(request);
    return 505;
  }

  const char *method = request->http_method;

  if (!strcmp(method, "HEAD"))
  {
    int head_ret = do_head(request, reply);
    reply_to_client(clientfd, reply);
    free_request(request);
    return head_ret;
  }
  else if (!strcmp(method, "GET"))
  {
    int get_ret = do_get(request, reply);
    reply_to_client(clientfd, reply);
    free_request(request);
    return get_ret;
  }
  else if (!strcmp(method, "POST"))
  {
    int post_ret = do_post(request, reply);
    reply_to_client(clientfd, reply);
    free_request(request);
    return post_ret;
  }
  else
  {
    send_response(reply, (char*)"501", (char*)"Not Implemented");
    reply_to_client(clientfd, reply);
    free_request(request);
    return 501;
  }

  // control should not reach here.
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

int send_msg(char *reply, const char* msg) {
  strcat(reply, msg);
  return 0;
}

int send_response(char *reply, const char *code, const char*reason) {
  sprintf(reply, "%s%s%s%s%s%s", http_version, sp, code, sp, reason, clrf);
  return 0;
}

int send_header(char *reply, const char *hname, const char *hvalue) {
  char header[512];
  sprintf(header, "%s%s%s%s%s%s", hname, sp, colon, sp, hvalue, clrf);
  strcat(reply, header);
  return 0;
}

/*
 * Handle HEAD request.
 * Return status code.
 */
int do_head(Request * request, char* reply) {
  char fullpath[4096];
  char extension[64];
  char mime_type[64];
  char curr_time[256];
  char last_modified[256];
  size_t content_length;
  char content_len_str[16];

  strcpy(fullpath, WWW_FOLDER);
  strcat(fullpath, request->http_uri);

  if (is_dir(fullpath)) {
    strcat(fullpath, default_index_file);
  }

  /* Check 404 Not Found error */
  if (access(fullpath, F_OK) < 0) {
    send_response(reply, "404", "Not Found");
    return 404;
  }

  /* Get content type */
  get_extension(fullpath, extension);
  str_tolower(extension);
  get_mime_type(extension, mime_type);

  /* Get content length */
  content_length = get_file_len(fullpath);
  sprintf(content_len_str, "%zu", content_length);

  /* Get current time */
  get_curr_time(curr_time, 256);

  /* Get last modified time */
  get_flmodified(fullpath, last_modified, 256);

  send_response(reply, "200", "OK");
  send_header(reply, "Connection", "close");
  send_header(reply, "Server", server_str);
  send_header(reply, "Date", curr_time);
  send_header(reply, "Content-Length", content_len_str);
  send_header(reply, "Content-Type", mime_type);
  send_header(reply, "Last-modified", last_modified);
  send_msg(reply, clrf);
  return 200;
}

int do_get(Request *request, char* reply) {
  return 0;
}

int do_post(Request *request, char* reply) {
  return 0;
}

/*
 * Get accordingly MIME type given file extension name
 */
void get_mime_type(const char *mime, char *type) {
  if (!strcmp(mime, "html")) {
    strcpy(type, "text/html");
  } else if (!strcmp(mime, "css")) {
    strcpy(type, "text/css");
  } else if (!strcmp(mime, "png")) {
    strcpy(type, "image/png");
  } else if (!strcmp(mime, "jpeg")) {
    strcpy(type, "image/jpeg");
  } else if (!strcmp(mime, "gif")) {
    strcpy(type, "image/gif");
  } else {
    strcpy(type, "application/octet-stream");
  }
}

/*
 * Get value given header name
 */
void get_header_value(Request *request, const char * hname, char *hvalue) {
  int i;

  for (i = 0; i < request->header_count; i++) {
    if (!strcmp(request->headers[i].header_name, hname)) {
      strcpy(hvalue, request->headers[i].header_value);
      return;
    }
  }
#ifdef DEBUG_VERBOSE
  console_log("[INFO][ERROR] Header name %s not found in request", hname);
#endif
}

/*
 * Free parsed request resources
 */
void free_request(Request *request) {
  free(request->headers);
  free(request);
}