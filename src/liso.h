/*
 * Created by XiaotongSun on 16/9/4.
 */

#ifndef LISO_H
#define LISO_H

#include "core/handle_clients.h"

#define USAGE "\nLiso Server Usage: ./lisod <HTTP port> <log file> <www folder>\n"

/* Command line parameters */
extern int port;
extern int https_port;
extern char *LOGFILE;
extern FILE *log_file;
extern char *LOCKFILE;
extern int lfd;
extern char *WWW_FOLDER;
extern char *CGI_scripts;
extern char *PRIVATE_KEY_FILE;
extern char *CERT_FILE;

/* Global socket */
extern int listenfd;
extern int ssl_socket;

/* Helper functions */
void sig_handler(int);
void daemonize();
SSL_CTX* ssl_init();
void install_signal_handler();
SSL* wrap_socket_with_ssl(int, SSL_CTX*);

#endif //LISO_H
