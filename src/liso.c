#include "liso.h"
#include "core/handle_clients.h"
#include "utilities/commons.h"

char *LOGFILE;
FILE *log_file;
char *WWW_FOLDER;
int listenfd;
client_pool pool;

void sig_handler(int signo) {
  console_log("called\n");
  switch (signo) {
    case SIGINT:
      tear_down();
      break;
    default:
      console_log("[Error] signo %d not handled", signo);
  }
}

int main(int args, char **argv) {

  /* Check usage */
  if (args < 2) {
    console_log(USAGE, argv[0]);
    exit(1);
  }

  /* Install signal handler */
  signal(SIGINT, sig_handler);

  /* Paramters from terminal */
  int port = atoi(argv[1]);   /* port param */
  LOGFILE = argv[2];          /* log file param */
  init_log();
  WWW_FOLDER = argv[3];       /* www folder param */
  create_folder(WWW_FOLDER, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  /* Temp parameters */
  int newfd;
  sockaddr_in clientaddr;
  socklen_t addrlen = sizeof(sockaddr_in);
  char remoteIP[INET6_ADDRSTRLEN];

  /* Create server's only listener socket */
  console_log("[INFO] ************Liso Echo Server*********");
  while ((listenfd = open_listenfd(port)) < 0);
  dump_log("[Main] Successfully create listener socket %d", listenfd);

  /* Init client pool */
  init_pool(listenfd);

  while(1) {
    dump_log("[Main] Selecting...");
    pool.read_fds = pool.master;
    pool.nready = select(pool.maxfd+1, &pool.read_fds, NULL, NULL, NULL);

    /* Handle exception in select, ignore all inormal cases */
    if (pool.nready <= 0) {
      if (pool.nready < 0) {
        err_sys("error in select");
      }
      continue;
    }

    if (FD_ISSET(listenfd, &pool.read_fds)) {
      /* Handle new incoming connections from client */
      newfd = accept(listenfd, (sockaddr *)&clientaddr, &addrlen);

      if (newfd < 0) {
        /* Ignore exception case in accept */
        err_sys("error when accepting new client connection");
        continue;
      }

      dump_log("[Main] New connection from %s on socket %d",
               inet_ntop(clientaddr.sin_family,
                         get_in_addr((sockaddr *) &clientaddr),
                         remoteIP,
                         INET6_ADDRSTRLEN),
               newfd);

      add_client_to_pool(newfd);

      if (pool.nready <= 0) continue;   /* No more readable descriptors */
    }
    handle_clients();
  }
}
