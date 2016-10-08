//
// Created by XiaotongSun on 16/9/17.
//

#ifndef INC_15_441_PROJECT_1_HANDLE_REQUEST_H
#define INC_15_441_PROJECT_1_HANDLE_REQUEST_H

#include "../parser/parse.h"
#include "handle_clients.h"
#include "../utilities/commons.h"
#include "../utilities/io.h"

typedef enum http_process_result{
  ERROR,
  CLOSE,
  PERSIST,
  NOT_ENOUGH_DATA
}http_process_result;

typedef struct host_and_port {
    char *host;
    int port;
} host_and_port;

http_process_result handle_http_request(int, dynamic_buffer *, size_t, host_and_port);
void reply_to_client(int, char*);
int check_http_version(Request *);

/* Response APIs */
int send_response(dynamic_buffer*, char*, char*);
int send_header(dynamic_buffer*, char*, char*);
int send_msg(dynamic_buffer*, char*);
int sen_msgbody(int, char *);

void get_mime_type(char*, char*);
char* get_header_value(Request*, char*);

void free_request(Request *);

int do_head(int client, Request *, dynamic_buffer *, int);
int do_get(int client, Request *, dynamic_buffer *, int);
int do_post(int client, Request *, dynamic_buffer *, int, char*, size_t);

void reply_400(dynamic_buffer *);   // Bad request
void reply_404(dynamic_buffer *);   // Not found
void reply_411(dynamic_buffer *);   // Length required
void reply_500(dynamic_buffer *);   // Internal error
void reply_501(dynamic_buffer *);   // Not implemented
void reply_505(dynamic_buffer *);   // Version unsupported

/* CGI */
#define CGI_ARGS_LEN 2
#define CGI_ENVP_LEN 22
#define CGI_ENVP_INFO_MAXLEN 1024
/* script path from command line */
char *CGI_scripts;

/* CGI related parameters */
typedef struct CGI_param {
    char * filename;
    char* args[CGI_ARGS_LEN];
    char* envp[CGI_ENVP_LEN];
} CGI_param;

CGI_param *init_CGI_param();
void build_CGI_param(CGI_param*, Request*, host_and_port);
void print_CGI_param(CGI_param*);
void free_CGI_param(CGI_param*);
char *new_string(char *);
void set_envp_field_by_str (char *, char *, char *, CGI_param*, int);
void set_envp_field_with_header (Request *, char *, char *, char *, CGI_param*, int);
#endif //INC_15_441_PROJECT_1_HANDLE_REQUEST_H
