//
// Created by XiaotongSun on 16/9/17.
//

#include "handle_request.h"

char *WWW_FOLDER;

/* Defineds tokens */
char *clrf = "\r\n";
char *sp = " ";
char *http_version = "HTTP/1.1";
char *colon = ":";

/* Constants string values */
char *default_index_file = "index.html";
char *server_str = "Liso/1.0";


/*
 * Return 0 when the current connection should be closed.
 * Return 1 when the current connection should be keeped
 * Return -1 on internal error.
 * The caller of this function should check the return value:
 * TODO: add documentation of return type
 */
http_process_result handle_http_request(int clientfd, dynamic_buffer* client_buffer, size_t header_len, host_and_port has){
#ifdef DEBUG_VERBOSE
  console_log("[INFO][HTTP] Client %d sent request:\n%sLength: %d", clientfd, client_buffer->buffer, strlen(client_buffer->buffer));
  console_log("[INFO][HTTP] Header Len: %d", header_len);
#endif

  /* default return value */
  http_process_result return_value = PERSIST;

  /* parse header */
  Request *request = NULL;
  request = parse(client_buffer->buffer, header_len);

  /* Handle 400 Error: Bad request */
  if (request == NULL) {
    console_log("Return 400 Bad request!!!");
    reply_400(client_buffer);
    return CLOSE;   //connection should be closed when encountering bad request.
  }

/*
#ifdef DEBUG_VERBOSE
  console_log("[INFO][PARSER] -------Parsed HTTP Request Begin--------");
  console_log("[Http Method] %s\n",request->http_method);
  console_log("[Http Version] %s\n",request->http_version);
  console_log("[Http Uri] %s\n",request->http_uri);
  int index;
  for(index = 0;index < request->header_count;index++){
    console_log("[Header name] %s : [Header Value] %s\n",request->headers[index].header_name,request->headers[index].header_value);
  }
  console_log("[INFO][PARSER] -------Parsed HTTP Request End--------");
#endif
*/

  /* Check Connection header. If 'connection: close' is
   * found, then should set return value to be 0, otherwise
   * set return value to be 1
   */
  char *connection_header_val;
  int last_conn = 0;
  connection_header_val = get_header_value(request, "Connection");
  if (!strcmp(connection_header_val, "close")) {
    return_value = CLOSE;
    last_conn = 1;
  }

  /* Handle 505 Error: Version not supported */
  int correct_version;
  correct_version = check_http_version(request);
  if (correct_version < 0) {
    reply_505(client_buffer);
    free_request(request);
    return CLOSE;
  }

  char* cgi_prefix = "/cgi/";
  char prefix[8]; memset(prefix, 0, 8);
  if (strlen(request->http_uri) > strlen(cgi_prefix))
    snprintf(prefix, strlen(cgi_prefix)+1, "%s", request->http_uri);

  if (!strcmp(cgi_prefix, prefix)) {       /* Handle dynamic http request */
    CGI_param* cgi_parameters = init_CGI_param();
    build_CGI_param(cgi_parameters, request, has);
    print_CGI_param(cgi_parameters);

    reply_501(client_buffer);

    free_CGI_param(cgi_parameters);
    return return_value;
  } else {                                 /* Handle statics http request */
    char *method = request->http_method;

    if (!strcmp(method, "HEAD"))
    {
      reset_dbuffer(client_buffer);
      do_head(clientfd, request, client_buffer, last_conn);
      free_request(request);
    }
    else if (!strcmp(method, "GET"))
    {
      reset_dbuffer(client_buffer);
      do_get(clientfd, request, client_buffer, last_conn);
      free_request(request);
    }
    else if (!strcmp(method, "POST"))
    {
      /* Check Conteng-Length header field */
      char *content_length_str;
      content_length_str = get_header_value(request, "Content-Length");
      if (strlen(content_length_str) == 0) {
        reply_411(client_buffer);
        free_request(request);
        return CLOSE;         // close current connection for 411 error.
      }

      size_t content_len = atoi(content_length_str);
      console_log("Content-len : %d + Header-len: %d : Buffer-len: %d", content_len, header_len, client_buffer->offset);
      if ((content_len+header_len) > client_buffer->offset)
        return NOT_ENOUGH_DATA;         // post method message body not fully received yet.

      do_post(clientfd, request, client_buffer, last_conn,
              client_buffer->buffer+header_len, content_len);
      free_request(request);
    }
    else
    {
      reply_501(client_buffer);
      free_request(request);
    }

    return return_value;
  }
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

int send_msg(dynamic_buffer *dbuf, char* msg) {
  append_content_dbuffer(dbuf, msg, strlen(msg));
  return 0;
}

int send_response(dynamic_buffer *dbuf, char *code, char*reason) {
  append_content_dbuffer(dbuf, http_version, strlen(http_version));
  append_content_dbuffer(dbuf, sp, strlen(sp));
  append_content_dbuffer(dbuf, code, strlen(code));
  append_content_dbuffer(dbuf, sp, strlen(sp));
  append_content_dbuffer(dbuf, reason, strlen(reason));
  append_content_dbuffer(dbuf, clrf, strlen(clrf));
  return 0;
}

int send_header(dynamic_buffer *dbuf, char *hname, char *hvalue) {
  append_content_dbuffer(dbuf, hname, strlen(hname));
  append_content_dbuffer(dbuf, colon, strlen(colon));
  append_content_dbuffer(dbuf, sp, strlen(sp));
  append_content_dbuffer(dbuf, hvalue, strlen(hvalue));
  append_content_dbuffer(dbuf, clrf, strlen(clrf));
  return 0;
}

int send_msgbody(dynamic_buffer *dbuf, char *body, size_t body_len) {
  append_content_dbuffer(dbuf, body, body_len);
  return 0;
}

/*
 * Handle HEAD request.
 * Return status code.
 */
int do_head(int client, Request * request, dynamic_buffer *client_buffer, int last_conn) {
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
#ifdef  DEBUG_VERBOSE
    console_log("File %s can not be accessed", fullpath);
#endif
   reply_404(client_buffer);
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

  send_response(client_buffer, "200", "OK");
  //send_header(client_buffer, "Connection", "close");
  send_header(client_buffer, "Server", server_str);
  send_header(client_buffer, "Date", curr_time);
  send_header(client_buffer, "Content-Length", content_len_str);
  send_header(client_buffer, "Content-Type", mime_type);
  send_header(client_buffer, "Last-modified", last_modified);
  if (last_conn)
    send_header(client_buffer, "connection", "close");
  else
    send_header(client_buffer, "connection", "keep-alive");
  send_msg(client_buffer, clrf);
#ifdef DEBUG_VERBOSE
  console_log("[DO_HEAD] Response to send: %s", client_buffer->buffer);
#endif
  return 200;
}

int do_get(int client, Request *request, dynamic_buffer *client_buffer, int last_conn) {
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
#endif
    reply_404(client_buffer);
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

  send_response(client_buffer, "200", "OK");
  //send_header(client_buffer, "Connection", "close");
  send_header(client_buffer, "Server", server_str);
  send_header(client_buffer, "Date", curr_time);
  send_header(client_buffer, "Content-Length", content_len_str);
  send_header(client_buffer, "Content-Type", mime_type);
  send_header(client_buffer, "Last-modified", last_modified);
  if (last_conn)
    send_header(client_buffer, "Connection", "close");
  else
    send_header(client_buffer, "Connection", "keep-alive");
  send_msg(client_buffer, clrf);

  size_t body_len;
  char * body = get_static_content(fullpath, &body_len);
  if (send_msgbody(client_buffer, body, body_len) < 0) {
    dump_log("[INFO][HTTP] Sending messages body to client %d failed", client);
#ifdef DEBUG_VERBOSE
    console_log("[INFO][HTTP][GET] Message body sending fails for client %d", client);
#endif
  }
  free_static_content(body, body_len);
  return 0;
}

int do_post(int client, Request *request, dynamic_buffer *client_buffer,
            int last_conn, char *message_body, size_t body_len) {
#ifdef DEBUG_VERBOSE
  console_log("[INFO][HTTP][POST] Client %d sent message body %s", client, message_body);
  console_log("[INFO][HTTP][POST] Message body length: %d", body_len);
#endif

  send_response(client_buffer, "200", "OK");
  send_msg(client_buffer, clrf);
  return 0;
}

/*
 * Get accordingly MIME type given file extension name
 */
void get_mime_type(char *mime, char *type) {
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
char* get_header_value(Request *request, char * hname) {
  int i;

  for (i = 0; i < request->header_count; i++) {
    if (!strcmp(request->headers[i].header_name, hname)) {
      return request->headers[i].header_value;
    }
  }
  return "";
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

void reply_400(dynamic_buffer *client_buffer){
  reset_dbuffer(client_buffer);
  send_response(client_buffer, (char*)"400", (char*)"Bad Request");
  send_header(client_buffer, "Connection", "close");
  send_msg(client_buffer, clrf);
}

void reply_404(dynamic_buffer *client_buffer){
  reset_dbuffer(client_buffer);
  send_response(client_buffer, "404", "Not Found");
  send_msg(client_buffer, clrf);
}

void reply_411(dynamic_buffer *client_buffer){
  reset_dbuffer(client_buffer);
  send_response(client_buffer, (char*)"411", (char*)"Length Required");
  send_header(client_buffer, "Connection", "close");
}

void reply_500(dynamic_buffer *client_buffer){
  reset_dbuffer(client_buffer);
  send_response(client_buffer, (char*)"500", (char*)"Internal Server Error");
  send_header(client_buffer, "Connection", "close");
}

void reply_501(dynamic_buffer *client_buffer){
  reset_dbuffer(client_buffer);
  send_response(client_buffer, (char*)"501", (char*)"Not Implemented");
  send_msg(client_buffer, clrf);
}

void reply_505(dynamic_buffer *client_buffer){
  reset_dbuffer(client_buffer);
  send_response(client_buffer, (char*)"505", (char*)"HTTP Version not supported");
  send_header(client_buffer, "Connection", "close");
  send_msg(client_buffer, clrf);
}

/* CGI */
CGI_param *init_CGI_param() {
  CGI_param * ret = (CGI_param*)malloc(sizeof(CGI_param));
  if (ret == NULL) {
    dump_log("[FATAL] Fails to allocate memory for CGI parametes");
    err_sys("[FATAL] Fails to allocate memory for CGI parametes");
  }
  return ret;
}

void build_CGI_param(CGI_param * param,  Request *request, host_and_port hap) {
  param->filename = CGI_scripts;
  param->args[0] = CGI_scripts;
  param->args[1] = NULL;

  char buf[CGI_ENVP_INFO_MAXLEN];
  int index = 0;
  set_envp_field_by_str("GATEWAY_INTERFACE", "CGI/1.1", buf, param, index++);
  set_envp_field_by_str("SERVER_SOFTWARE", "Liso/1.0", buf, param, index++);
  set_envp_field_by_str("SERVER_PROTOCOL", "HTTP/1.1", buf, param, index++);
  set_envp_field_by_str("REQUEST_METHOD", request->http_method, buf, param, index++);
  set_envp_field_by_str("REQUEST_URI", request->http_uri, buf, param, index++);
  set_envp_field_by_str("SCRIPT_NAME", "/cgi", buf, param, index++);

  char path_info[512];  memset(path_info, 0, 512);
  char query_string[512]; memset(query_string, 0, 512);
  size_t split = strstr(request->http_uri, "?") - request->http_uri;

  memcpy(query_string, request->http_uri+split+1, strlen(request->http_uri)-split-1);
  set_envp_field_by_str("QUERY_STRING", query_string, buf, param, index++);

  memcpy(path_info, request->http_uri+4, split-4);
  set_envp_field_by_str("PATH_INFO", path_info, buf, param, index++);

  /* envp taken from request */

  set_envp_field_with_header(request, "Content-Length", "CONTENT_LENGTH", buf, param, index++);
  set_envp_field_with_header(request, "Content-Type", "CONTENT_TYPE", buf, param, index++);
  set_envp_field_with_header(request, "Accept", "HTTP_ACCEPT", buf, param, index++);
  set_envp_field_with_header(request, "Referer", "HTTP_REFERER", buf, param, index++);
  set_envp_field_with_header(request, "Accept-Encoding", "HTTP_ACCEPT_ENCODING", buf, param, index++);
  set_envp_field_with_header(request, "Accept-Language", "HTTP_ACCEPT_LANGUAGE", buf, param, index++);
  set_envp_field_with_header(request, "Accept-Charset", "HTTP_ACCEPT_CHARSET", buf, param, index++);
  set_envp_field_with_header(request, "Cookie", "HTTP_COOKIE", buf, param, index++);
  set_envp_field_with_header(request, "User-Agent", "HTTP_USER_AGENT", buf, param, index++);
  set_envp_field_with_header(request, "Host", "HTTP_HOST", buf, param, index++);
  set_envp_field_with_header(request, "Connection", "HTTP_CONNECTION", buf, param, index++);

  set_envp_field_by_str("REMOTE_ADDR", hap.host, buf, param, index++);
  char port_str[8];
  memset(port_str, 0, 8);
  sprintf(port_str, "%d", hap.port);
  set_envp_field_by_str("SERVER_PORT", port_str, buf, param, index++);
  param->envp[index++] = NULL;
}

void print_CGI_param(CGI_param * param) {
  console_log("-------CGI Param------");
  console_log("[Filename]: %s", param->filename);
  console_log("[Args]: %s", param->args[0]);
  console_log("[Envp]:");
  int i = 0;
  while(param->envp[i] != NULL) {
    console_log("%d : %s", i+1, param->envp[i]);
    i++;
  }
}

char *new_string(char *str) {
  char * buf = (char*)malloc(strlen(str)+1);
  memcpy(buf, str, strlen(str));
  return buf;
}

void set_envp_field_by_str (char *envp_name, char *value, char *buf, CGI_param*param, int index) {
  memset(buf, 0, CGI_ENVP_INFO_MAXLEN);
  sprintf(buf, "%s=%s", envp_name, value);
  param->envp[index] = new_string(buf);
}

void set_envp_field_with_header (Request *request, char *header, char *envp_name, char *buf, CGI_param* param, int index) {
  char *value = get_header_value(request, header);
  memset(buf, 0, CGI_ENVP_INFO_MAXLEN);
  sprintf(buf, "%s=%s", envp_name, value);
  param->envp[index] = new_string(buf);
}

void free_CGI_param(CGI_param* param) {
  int i = 0;
  while(param->envp[i] != NULL)  {
    free(param->envp[i]);
    i++;
  }
  free(param);
}