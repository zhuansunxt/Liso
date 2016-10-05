/*
 * Created by XiaotongSun on 16/9/4.
 */

#ifndef LISO_H
#define LISO_H

#include "core/handle_clients.h"

#define USAGE "\nLiso Server Usage: %s <HTTP port> <log file> <www folder>\n"

extern char *LOGFILE;
extern FILE *log_file;
extern char *LOCKFILE;
extern char *WWW_FOLDER;
extern char *CGI_scripts;
extern char *PRIVATE_KEY_FILE;
extern char *CERT_FILE;

extern int listenfd;

void daemonize();

#endif //LISO_H
