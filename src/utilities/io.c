/**
 * IO:
 *  This file contains all I/O related functions's implementations, including interfaces dealing
 *  with network sockets and disk files.
 */
#include "io.h"

/*
 * Get sockaddr, IPv4 or IPv6.
 */
void *get_in_addr(sockaddr *sa){
  if (sa->sa_family == AF_INET) {
    return &(((sockaddr_in *)sa)->sin_addr);
  }
  return &(((sockaddr_in6 *)sa)->sin6_addr);
}

/*
 * Open and return listener socket on given port.
 * Return socket descriptor on success.
 * Return -1 and set errno on failure.
 */
int open_listenfd(int port){
  int listenfd;
  int yes = 1;
  sockaddr_in serveraddr;

  /* Create socket descriptor for listening usage */
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    console_log("%s create listener socket", SOCKET_API_ERR_MSG);
    return -1;
  }

  /* Eliminates "Address already in use" error from bind */
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
    console_log("%s set SO_REUSEADDR", SOCKET_API_ERR_MSG);
    return -1;
  }

  /* Bind listener socket */
  memset(&serveraddr, 0, sizeof(sockaddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)port);

  if (bind(listenfd, (sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
    console_log("%s bind listenser socket", SOCKET_API_ERR_MSG);
    close(listenfd);
    return -1;
  }

  if (listen(listenfd, 5) < 0) {
    console_log("%s listen on listener socket", SOCKET_API_ERR_MSG);
    return -1;
  }

  return listenfd;
}

int open_ssl_socket(int port, SSL_CTX* ssl_context) {
  int   ssl_socket;
  int yes;
  sockaddr_in addr;

  if ((ssl_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    SSL_CTX_free(ssl_context);
    fprintf(stderr, "Failed creating socket.\n");
    return -1;
  }

  /* Eliminates "Address already in use" error from bind */
  if (setsockopt(ssl_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
    console_log("%s set SO_REUSEADDR", SOCKET_API_ERR_MSG);
    return -1;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(ssl_socket, (sockaddr *)&addr, sizeof(addr)) < 0)
  {
    close(ssl_socket);
    SSL_CTX_free(ssl_context);
    fprintf(stderr, "Failed binding SSL socket.\n");
    return -1;
  }

  if (listen(ssl_socket, 5) < 0) {
    fprintf(stderr, "Failed listening to SSL socket.\n");
    return -1;
  }

  return ssl_socket;
}

/*
 * Wrapper for send(): send n bytes.
 * guarantee to send all data given length n.
 * Return number of bytes that are sent on success.
 * Return -1 on error.
 */
ssize_t sendn(int fd, const void *vptr, ssize_t n) {
  ssize_t nleft = n;
  ssize_t nwritten = 0;
  const char *ptr = vptr;

  while (nleft > 0) {
    if ((nwritten = send(fd, ptr+nwritten, n, 0)) < 0) {
      if (nwritten < 0 && errno == EINTR) {
        nwritten = 0;   /* ignore the interrupt */
      } else {
        console_log("send n error: %s", strerror(errno));
        return -1;    /* error */
      }
    }

    nleft -= nwritten;
    ptr += nwritten;
  }

  return n;
}

/*
 * Wrapper for sendn
 * Will termiate when error in system call.
 */
void Sendn(int fd, const void *ptr, int n) {
  ssize_t ret;
  if ((ret = sendn(fd, ptr, n)) != n) {
    console_log("sendn returns %d != n %d", ret, n);
    err_sys("sendn error");
  }
}

/*
 * Wrapper for mkdir sys call.
 * If the directory already exists, do nothing.
 */
int create_folder(const char *path, mode_t mode) {
  int ret;
  struct stat st = {0};

  if (stat(path, &st) != -1) return 0;    /* If already exists then return */

  ret = mkdir(path, mode);
  if (ret < 0) {
    err_sys("mkdir error");
  }
  return ret;
}

/*
 * Check whether a file path is directory.
 * Return 1 if it is, 0 otherwise.
 */
int is_dir(const char *path) {
  size_t len = strlen(path);
  if (path[len-1] == '/') {
    return 1;
  }
  return 0;
}

/*
 * Return extension name given file path.
 * TODO: Implementation is ad-hoc. Need further testing.
 */
void get_extension(const char *path, char *result) {
  int extension_max_len = 10;
  size_t len = strlen(path);
  int i;
  for (i = len-1; i >= 0 && (len-i) < extension_max_len; i--) {
    int curr_len = len-i;
    if (path[i] == '.') {
      strncpy(result, path +(len-curr_len)+1, curr_len-1);
      return ;
    }
  }
  strncpy(result, "none", 4);
}

/*
 * Convert all chars of a str to lower case in place.
 */
void str_tolower(char *str) {
  int i;
  for (i = 0; str[i]; i++) {
    str[i] = tolower(str[i]);
  }
}

/*
 * Get file content length given file path.
 */
size_t get_file_len(const char* fullpath) {
  struct stat st;
  stat(fullpath, &st);
  return st.st_size;
}

/*
 * Get current time.
 */
void get_curr_time(char *time_buf, size_t buf_size) {
  time_t raw_time;
  struct tm * timeinfo;

  time(&raw_time);
  timeinfo = localtime(&raw_time);
  strftime(time_buf, buf_size, "%a, %d %b %Y %H:%M:%S %Z", timeinfo);
}

/*
 * Get file's last modified name given file path.
 */
void get_flmodified(const char*path, char *last_mod_time, size_t buf_size) {
  struct stat st;
  struct tm *curr_gmt_time = NULL;
  stat(path, &st);
  curr_gmt_time = gmtime(&st.st_mtime);
  strftime(last_mod_time, buf_size, "%a, %d %b %Y %H:%M:%S %Z", curr_gmt_time);
}

char *get_static_content(char *path, size_t *content_len){
  char *file_to_send;
  size_t file_len;

  int fd = open(path, O_RDONLY, (mode_t)0600);
  if (fd == -1) {
    dump_log("[IO][ERROR] Error when opening file %s for reading", path);
    return NULL;
  }

  struct stat file_st = {0};
  if ((fstat(fd, &file_st)) == -1) {
    dump_log("[IO][ERROR] Error when getting status of file %s for reading", path);
    close(fd);
    return NULL;
  }

  file_len = file_st.st_size;
  if (file_len <= 0) {
    dump_log("[IO][ERROR] File %s is empty. Noting to read from", path);
    close(fd);
    return NULL;
  }

  file_to_send = mmap(0, file_len, PROT_READ, MAP_PRIVATE, fd, 0);
  if (file_to_send == MAP_FAILED) {
    close(fd);
    dump_log("[IO][ERROR] File %s mapping to RAM fails: %s", path, strerror(errno));
    return NULL;
  }

  close(fd);
  *content_len = file_len;
  return file_to_send;
}

void free_static_content(char * file_buffer, size_t file_len) {
  if (munmap(file_buffer, file_len) == -1) {
    dump_log("[IO][ERROR] Fails to clean static content buffer");
  }
}

void init_dbuffer(dynamic_buffer *dbuf) {
  dbuf->buffer = (char*)malloc(sizeof(char) * BUF_SIZE);
  dbuf->offset = 0;
  dbuf->capacity = BUF_SIZE;
  dbuf->send_offset = 0;
  memset(dbuf->buffer, 0, BUF_SIZE);
}

void append_content_dbuffer(dynamic_buffer *dbuf, char *buf, ssize_t len) {
  if ((dbuf->offset+len) > dbuf->capacity) {
    int expansion_size = dbuf->capacity;
    while ((dbuf->offset+len) > expansion_size) expansion_size *= 2;
    dbuf->buffer = (char *) realloc(dbuf->buffer, expansion_size);
    dbuf->capacity = expansion_size;
  }
  memcpy(dbuf->buffer+dbuf->offset, buf, len);
  dbuf->offset += len;
}

void reset_dbuffer(dynamic_buffer *dbuf) {
  memset(dbuf->buffer, 0, dbuf->capacity);
  dbuf->offset = 0;
  dbuf->send_offset = 0;
}

void print_dbuffer(dynamic_buffer *dbuf) {
  if (dbuf == NULL) {
    console_log("Empty Buffer");
  } else {
    console_log("Buffer offset: %d", dbuf->offset);
    console_log("Buffer capacity: %d", dbuf->capacity);
    console_log("Buffer send offset: %d", dbuf->send_offset);
    console_log("Buffer content %s", dbuf->buffer);
  }
}

void free_dbuffer(dynamic_buffer *dbuf) {
  free(dbuf->buffer);
  free(dbuf);
}
//0/* Another version */
//int write_file_to_socket(int clientfd, char *path) {
//  char buf[BUF_SIZE];
//  memset(buf, 0, BUF_SIZE);
//  int fd = open(path, O_RDONLY, (mode_t)0600);
//  ssize_t nread;
//  while((nread = read(fd, buf, BUF_SIZE)) > 0) {
//    Sendn(clientfd, buf, nread);
//  }
//  return 0;
//}