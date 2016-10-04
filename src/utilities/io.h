#ifndef LISO_IO_H
#define LISO_IO_H

#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <ctype.h>
#include <time.h>

#include "commons.h"

#define SOCKET_API_ERR_MSG "[Error in socket_api]"
#define MIN(a,b) (((a)<(b))?(a):(b))

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
int write_file_to_socket(int, char *);
char *get_static_content(char *, size_t *);
void free_static_content(char *, size_t);
/*--------------- End File System APIs -----------*/


typedef struct dynamic_buffer{
  char *buffer;
  size_t offset;
  size_t capacity;
  size_t send_offset;
} dynamic_buffer;

void init_dbuffer(dynamic_buffer *);
void append_content_dbuffer(dynamic_buffer *, char *buf, ssize_t len);
void reset_dbuffer(dynamic_buffer *);
void free_dbuffer(dynamic_buffer *);

#endif //LISO_IO_H
