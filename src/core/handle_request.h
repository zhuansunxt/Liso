//
// Created by XiaotongSun on 16/9/17.
//

#ifndef INC_15_441_PROJECT_1_HANDLE_REQUEST_H
#define INC_15_441_PROJECT_1_HANDLE_REQUEST_H

#include "../parser/parse.h"
#include "handle_clients.h"
#include "../utilities/commons.h"
#include "../utilities/io.h"

int handle_http_request(int, dynamic_buffer *, size_t);
void reply_to_client(int, char*);
int check_http_version(Request *);
//int send_error(char*, char*, char*);

/* Response APIs */
int send_response(dynamic_buffer*, char*, char*);
int send_header(dynamic_buffer*, char*, char*);
int send_msg(dynamic_buffer*, char*);
int sen_msgbody(int, char *);

void get_mime_type(char*, char*);
void get_header_value(Request*, char*, char*);

void free_request(Request *);

int do_head(int client, Request *, dynamic_buffer *, int);
int do_get(int client, Request *, dynamic_buffer *, int);
int do_post(int client, Request *, dynamic_buffer *, int);
#endif //INC_15_441_PROJECT_1_HANDLE_REQUEST_H
