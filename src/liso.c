#include "liso.h"
#include "core/handle_clients.h"
#include "utilities/commons.h"

char *LOGFILE;
FILE *log_file;
char *LOCKFILE;
char *WWW_FOLDER;
int listenfd;
char *CGI_scripts;
char *PRIVATE_KEY_FILE;
char *CERT_FILE;
client_pool pool;

void sig_handler(int signo) {
  console_log("called\n");
  switch (signo) {
    case SIGTERM:
    case SIGINT:
      tear_down();
      clear_pool();
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
  signal(SIGTERM, sig_handler);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGHUP, SIG_IGN);

  /* Paramters from terminal */
  //<HTTP port> <HTTPS port> <log file> <lock file> <www folder>
  //<CGI script path> <private key file> <certificate file>
  int port = atoi(argv[1]);   /* port param */
  //int https_port = atoi(argv[2]); /* https port param */
  LOGFILE = argv[3];          /* log file param */
  LOCKFILE = argv[4];         /* lock file param */
  WWW_FOLDER = argv[5];       /* www folder param */
  CGI_scripts = argv[6];      /* cgi script param */
  PRIVATE_KEY_FILE = argv[7]; /* private key file param */
  CERT_FILE = argv[8];        /* certificate file param */

  /* Daemonize the process */
#ifdef DEBUG_VERBOSE
  daemonize();
#endif

  /* Let server process be mutual exclusion */
  int lfd;
  char lstr[8];
  lfd = open(LOCKFILE, O_RDWR|O_CREAT, 0640);
  if (lfd < 0) {
    fprintf(stderr, "[Main][Error] Can not open lock file");
    exit(EXIT_FAILURE);
  }
  if (lockf(lfd, F_TLOCK, 0) < 0) {
    fprintf(stderr, "[Main][Error] Can not lock on the lock file");
    exit(EXIT_FAILURE);
  }
  sprintf(lstr, "%d\n", getpid());
  write(lfd, lstr, strlen(lstr));

  /* Global initializations */
  init_log();
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
    //dump_log("[Main] Selecting...");
    pool.read_fds = pool.master;
    pool.write_fds = pool.master;
    pool.nready = select(pool.maxfd+1, &pool.read_fds, &pool.write_fds, NULL, NULL);

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

void daemonize() {
  pid_t pid;
  int i;
  int fd_0, fd_1, fd_2;

  /* Detach from parent's controlling tty */
  if ((pid = fork() < 0)) {
    console_log("[Main][Error] Can not fork!");
    exit(EXIT_FAILURE);
  } else if (pid > 0) {
    exit(EXIT_SUCCESS);
  }
  setsid();

  /* Close all open descriptors inherited */
  for (i = getdtablesize(); i >= 0; i--)
    close(i);

  /* Attach file descriptors 0, 1, and 2 to /dev/null. */
  fd_0 = open ("/dev/null", O_RDWR);
  fd_1 = dup (0);
  fd_2 = dup (0);
}
