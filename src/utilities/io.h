#ifndef LISO_IO_H
#define LISO_IO_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <ctype.h>
#include <time.h>

#include "commons.h"

#define SOCKET_API_ERR_MSG "[Error in socket_api]"

/*--------------- Socket APIs ---------------*/
typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr_in6 sockaddr_in6;
typedef struct sockaddr_storage sockaddr_storage;
typedef struct addrinfo addrinfo;

void *get_in_addr(sockaddr *sa);
int open_listenfd(int port);
ssize_t sendn(int, const void *, ssize_t);
void Sendn(int, const void *, int);
/*--------------- End Socket APIs ------------*/

/*--------------- File System APIs ---------------*/
int create_folder(const char *, mode_t);
int is_dir(const char *path);
void get_extension(const char *, char*);
void str_tolower(char *);
void get_curr_time(char *, size_t);
size_t get_file_len(const char*);
void get_flmodified(const char*, char *, size_t);
/*--------------- End File System APIs -----------*/

#endif //LISO_IO_H
