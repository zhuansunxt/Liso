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
 * Return 0 when the current connection should be closed.
 * Return 1 when the current connection should be keeped
 * Return -1 on internal error.
 * The caller of this function should check the return value:
 *   - If 1, it means that persistent connection should be keeped.
 *     Caller should reset client's state for receiving request.
 *   - If 0, it means that the current connection should be closed.
 *     Caller should clear client's state and free all associated
 *     resources.
 *   - If -1, the caller should handle the error type, clear
 *     client's state and free all associated resources.
 */
int handle_http_request(int clientfd, char *buf, ssize_t len){
#ifdef DEBUG_VERBOSE
  console_log("[INFO][HTTP] Client %d sent request:\n%sLength: %d", clientfd, buf, strlen(buf));
#endif

  /* default return value */
  int return_value = 1;

  /* reply buffer */
  char reply[BUF_SIZE];
  memset(reply, 0, sizeof(char)*BUF_SIZE);

  /* parse header */
  Request *request = NULL;
  request = parse(buf, len);

  /* Handle 400 Error: Bad request */
  if (request == NULL) {
    /* if parsing fails, send back 400 error */
    send_response(reply, (char*)"400", (char*)"Bad Request");
    send_msg(reply, clrf);
    reply_to_client(clientfd, reply);
    return 0;   //connection should be closed when encountering bad request.
  }

#ifdef DEBUG_VERBOSE
  console_log("[INFO][PARSER] -------Parsed HTTP Request--------");
  console_log("[Http Method] %s\n",request->http_method);
  console_log("[Http Version] %s\n",request->http_version);
  console_log("[Http Uri] %s\n",request->http_uri);
  int index;
  for(index = 0;index < request->header_count;index++){
    console_log("[Request Header]\n");
    console_log("[Header name] %s : [Header Value] %s\n",request->headers[index].header_name,request->headers[index].header_value);
  }
  console_log("[INFO][PARSER] -------Parsed HTTP Request--------");
#endif

  /* Check Connection header. If 'connection: close' is
   * found, then shoud set return value to be 0, otherwise
   * set return value to be 1
   */
  char connection_header_val[32];
  memset(connection_header_val, 0, sizeof(connection_header_val));
  get_header_value(request, "Connection", connection_header_val);
  if (!strcmp(connection_header_val, "close")) return_value = 0;

  /* Handle 505 Error: Version not supported */
  int correct_version;
  correct_version = check_http_version(request);
  if (correct_version < 0) {
    /* if checking version fails, send back 505 error */
    send_response(reply, (char*)"505", (char*)"HTTP Version not supported");
    send_msg(reply, clrf);
    reply_to_client(clientfd, reply);
    free_request(request);
    return return_value;
  }

  const char *method = request->http_method;

  if (!strcmp(method, "HEAD"))
  {
    do_head(clientfd, request, reply);
    free_request(request);
  }
  else if (!strcmp(method, "GET"))
  {
    do_get(clientfd, request, reply);
    free_request(request);
  }
  else if (!strcmp(method, "POST"))
  {
    do_post(clientfd, request, reply);
    free_request(request);
  }
  else
  {
    send_response(reply, (char*)"501", (char*)"Not Implemented");
    send_header(reply, "Connection", "close");
    send_msg(reply, clrf);
    reply_to_client(clientfd, reply);
    free_request(request);
    /* Close the socket when 501 encountered */
    return_value = 0;
  }

  return return_value;
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
  sprintf(header, "%s%s%s%s%s", hname, colon, sp, hvalue, clrf);
  strcat(reply, header);
  return 0;
}

int send_msgbody(int client, char *fullpath) {
  return write_file_to_socket(client, fullpath);
}

/*
 * Handle HEAD request.
 * Return status code.
 */
int do_head(int client, Request * request, char* reply) {
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
    return 0;
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
  //send_header(reply, "Connection", "close");
  send_header(reply, "Server", server_str);
  send_header(reply, "Date", curr_time);
  send_header(reply, "Content-Length", content_len_str);
  send_header(reply, "Content-Type", mime_type);
  send_header(reply, "Last-modified", last_modified);
  send_msg(reply, clrf);
  reply_to_client(client, reply);
  return 200;
}

int do_get(int client, Request *request, char* reply) {
  char fullpath[4096];
  char extension[64];
  char mime_type[64];
  char curr_time[256];
  char last_modified[256];
  size_t content_length;
  char content_len_str[16];

  strcpy(fullpath, WWW_FOLDER);
  strcat(fullpath, request->http_uri);

  if (is_dir(fullpath)) strcat(fullpath, default_index_file);

  /* Check 404 Not Found Error */
  if (access(fullpath, F_OK) < 0) {
#ifdef DEBUG_VERBOSE
    console_log("Path %s can not be accessed", fullpath);
    send_response(reply, "404", "Not Found");
    reply_to_client(client, reply);
#endif
    return 0;
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
  //send_header(reply, "Connection", "close");
  send_header(reply, "Server", server_str);
  send_header(reply, "Date", curr_time);
  send_header(reply, "Content-Length", content_len_str);
  send_header(reply, "Content-Type", mime_type);
  send_header(reply, "Last-modified", last_modified);
  send_msg(reply, clrf);
  reply_to_client(client, reply);
  if (send_msgbody(client, fullpath) < 0) {
    dump_log("[INFO][HTTP] Sending messages body to client %d failed", client);
#ifdef DEBUG_VERBOSE
    console_log("[INFO][HTTP][GET] Message body sending fails for client %d", client);
#endif
  }
  return 0;
}

int do_post(int client, Request *request, char* reply) {
  char content_length[32];
  memset(content_length, 0, sizeof(content_length));
  get_header_value(request, "Content-Length", content_length);
  if (strlen(content_length) == 0) {
    /* header value not found */
    send_response(reply, "401", "Length Required");
    send_msg(reply, clrf);
    reply_to_client(client, reply);
    return 0;
  }

  send_response(reply, "200", "OK");
  send_msg(reply, clrf);
  reply_to_client(client, reply);
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
 * Get value given header name.
 * If header name not found, hvalue param will not be set.
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