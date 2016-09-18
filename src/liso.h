/*
 * Created by XiaotongSun on 16/9/4.
 */

#ifndef LISO_H
#define LISO_H

#include "core/handle_clients.h"

#define USAGE "\nLiso Server Usage: %s <HTTP port> <log file> <www folder>\n"

extern char *LOGFILE;
extern FILE *log_file;
extern char *WWW_FOLDER;
extern int listenfd;

#endif //LISO_H
