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
  PENDING_REQUEST,
  PERSIST,
  NOT_ENOUGH_DATA,    /* for POST method */
  CGI_READY_FOR_READ,
  CGI_READY_FOR_WRITE,
  CGI_READY_FOR_READ_CLOSE,
  CGI_READY_FOR_WRITE_CLOSE
}http_process_result;

typedef struct host_and_port {
    char *host;
    int port;
} host_and_port;

http_process_result handle_http_request(int, dynamic_buffer *, size_t, host_and_port, dynamic_buffer*);
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

typedef struct CGI_executor {
  int clientfd;
  int stdin_pipe[2];    /* { write data --> stdin_pipe[1] } -> { stdin_pipe[0] --> stdin } */
  int stdout_pipe[2];   /* { read data <--  stdout_pipe[0] } <-- {stdout_pipe[1] <-- stdout } */
  dynamic_buffer* cgi_buffer;
  CGI_param* cgi_parameter;
} CGI_executor;

typedef struct CGI_pool {
  CGI_executor* executors[FD_SETSIZE];
} CGI_pool;

extern CGI_pool * cgi_pool;

CGI_param *init_CGI_param();
void build_CGI_param(CGI_param*, Request*, host_and_port);
void print_CGI_param(CGI_param*);
void free_CGI_param(CGI_param*);
char *new_string(char *);
void set_envp_field_by_str (char *, char *, char *, CGI_param*, int);
void set_envp_field_with_header (Request *, char *, char *, char *, CGI_param*, int);

void init_CGI_pool();
//CGI_executor* init_CGI_executor();
//int add_CGI_executor_to_pool(int cliendfd, CGI_param * cgi_parameter);
void free_CGI_executor(CGI_executor *executor);
void free_CGI_pool();
void print_executor(CGI_executor *);
void print_CGI_pool();
CGI_executor * get_CGI_executor_by_client(int clientfd);
void clear_CGI_executor_by_client(int clientfd);

void execve_error_handler();
void handle_dynamic_request(int cliendfd, CGI_param *cgi_parameter, char *post_body, size_t content_length);
#endif //INC_15_441_PROJECT_1_HANDLE_REQUEST_H
