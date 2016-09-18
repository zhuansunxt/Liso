//
// Created by XiaotongSun on 16/9/17.
//

#ifndef INC_15_441_PROJECT_1_HANDLE_REQUEST_H
#define INC_15_441_PROJECT_1_HANDLE_REQUEST_H

#include "../parser/parse.h"
#include "handle_clients.h"
#include "../utilities/commons.h"
#include "../utilities/io.h"

int handle_http_request(int, char*, ssize_t);
void reply_to_client(int, char*);
int check_http_version(Request *);
//int send_error(char*, char*, char*);

/* Response APIs */
int send_response(char*, const char*, const char*);
int send_header(char*, const char*, const char*);
int send_msg(char*, const char*);

void get_mime_type(const char*, char*);
void get_header_value(Request*, const char*, char*);

int do_head(Request *, char *);
int do_get(Request *, char *);    //TODO
int do_post(Request *, char *);   //TODO
#endif //INC_15_441_PROJECT_1_HANDLE_REQUEST_H
