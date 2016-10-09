#include "liso.h"
#include "core/handle_clients.h"
#include "utilities/commons.h"

/* Command line parameters */
int port;
int https_port;
char *LOGFILE;
FILE *log_file;
char *LOCKFILE;
int lfd;
char *WWW_FOLDER;
char *CGI_scripts;
char *PRIVATE_KEY_FILE;
char *CERT_FILE;

/* Global client pool */
client_pool pool;

/* Global socket */
int listenfd;
int ssl_socket;

int main(int args, char **argv) {

  /* Check usage */
  if (args < 8) {
    fprintf(stdout, USAGE);
    exit(1);
  }

  /* Read parameters from command line arguments */
  port = atoi(argv[1]);   /* port param */
  https_port = atoi(argv[2]); /* https port param */
  LOGFILE = argv[3];          /* log file param */
  LOCKFILE = argv[4];         /* lock file param */
  WWW_FOLDER = argv[5];       /* www folder param */
  CGI_scripts = argv[6];      /* cgi script param */
  PRIVATE_KEY_FILE = argv[7]; /* private key file param */
  CERT_FILE = argv[8];        /* certificate file param */

  /* Daemonize the process */
#ifndef DEBUG_VERBOSE
  daemonize();
#endif

  /* Install signal handler */
  install_signal_handler();

  /* Let server process be mutual exclusive: lock on lock file */
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

  /* HTTP network resource parameters */
  int newfd;
  sockaddr_in clientaddr;
  socklen_t addrlen = sizeof(sockaddr_in);
  char remoteIP[INET6_ADDRSTRLEN];

  /* SSL initialization */
  SSL_CTX *ssl_context;
  if ((ssl_context = ssl_init()) == NULL) {
    console_log("[Main][Error] SSL init fails");
    tear_down();
    exit(EXIT_FAILURE);
  }

  /* Create server's only listener socket */
  while ((listenfd = open_listenfd(port)) < 0);
  //while (ssl_socket = open_listenfd(https_port) < 0);
  while ((ssl_socket = open_ssl_socket(https_port, ssl_context)) < 0);

  /* Init client pool */
  init_pool();

  console_log("[INFO] ************Liso Echo Server*********");
  while(1) {
    pool.read_fds = pool.master;
    pool.write_fds = pool.master;
    pool.nready = select(pool.maxfd+1, &pool.read_fds, &pool.write_fds, NULL, NULL);

    /* Handle exception in select, ignore all inormal cases */
    if (pool.nready <= 0) {
      if (pool.nready < 0) {
        tear_down();
        err_sys("error in select");
      }
      continue;
    }

    if (FD_ISSET(listenfd, &pool.read_fds)) {
      /* Handle new incoming connections from http port */
      console_log("[Main] Incoming HTTP client!");
      newfd = accept(listenfd, (sockaddr *) &clientaddr, &addrlen);
      if (newfd < 0) {
#ifdef DEBUG_VERBOSE
        console_log("error when accepting new client connection from HTTP port %d", port);
#endif
        dump_log("[ERROR] error when accepting new client connection from HTTP port %d", port);
        continue;
      }

      char *remote_addr = (char*)inet_ntop(clientaddr.sin_family,
                                    get_in_addr((sockaddr *) &clientaddr),
                                    remoteIP,
                                    INET6_ADDRSTRLEN);
      add_client_to_pool(newfd, NULL, HTTP_CLIENT, remote_addr);
      if (pool.nready <= 0) continue;   /* No more readable descriptors */
    }

    if (FD_ISSET(ssl_socket, &pool.read_fds)) {
      /* Handle new  incomming connecitions from https port */
      console_log("[Main] Incoming HTTPS client!");
      newfd = accept(ssl_socket, (sockaddr *) &clientaddr, &addrlen);
      if (newfd < 0) {
#ifdef DEBUG_VERBOSE
        console_log("error when accepting new client connection from HTTPS port %d", https_port);
#endif
        dump_log("[ERROR] error when accepting new client connection from HTTP port %d", https_port);
        continue;
      }

      /* wrap socket with ssl */
      SSL *client_context;

      if ((client_context = wrap_socket_with_ssl(newfd, ssl_context)) == NULL) {
        console_log("error when wrapping new client connection from HTTPS port");
        continue;
      }

      char *remote_addr = (char*)inet_ntop(clientaddr.sin_family,
                                    get_in_addr((sockaddr *) &clientaddr),
                                    remoteIP,
                                    INET6_ADDRSTRLEN);
      add_client_to_pool(newfd, client_context, HTTPS_CLIENT, remote_addr);
      if (pool.nready <= 0) continue;   /*No more readable descriptors */
    }

    /* Handle client request : read and write */
    handle_clients();
  }
}

/* Daemonze the server process */
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

/* Intialize SSL given context */
SSL_CTX *ssl_init() {
  SSL_CTX * ssl_context;
  SSL_load_error_strings();
  SSL_library_init();

  /* we want to use TLSv1 only */
  if ((ssl_context = SSL_CTX_new(TLSv1_server_method())) == NULL)
  {
    fprintf(stderr, "Error creating SSL context.\n");
    return NULL;
  }

  /* register private key */
  console_log("Private key file : %s", PRIVATE_KEY_FILE);
  if (SSL_CTX_use_PrivateKey_file(ssl_context, PRIVATE_KEY_FILE, SSL_FILETYPE_PEM) == 0)
  {
    SSL_CTX_free(ssl_context);
    fprintf(stderr, "Error associating private key.\n");
    return NULL;
  }

  /* register public key (certificate) */
  console_log("Cerficate file : %s", CERT_FILE);
  if (SSL_CTX_use_certificate_file(ssl_context, CERT_FILE, SSL_FILETYPE_PEM) == 0)
  {
    SSL_CTX_free(ssl_context);
    fprintf(stderr, "Error associating certificate.\n");
    return NULL;
  }

  return ssl_context;
}

/* Wrap the client socket with SSL */
SSL *wrap_socket_with_ssl(int client_sock, SSL_CTX *ssl_context) {
  SSL *client_context;

  console_log("New client socket: %d", client_sock);
  if ((client_context = SSL_new(ssl_context)) == NULL)
  {
    fprintf(stderr, "Error creating client SSL context.\n");
    return NULL;
  }

  if (SSL_set_fd(client_context, client_sock) == 0)
  {
    SSL_free(client_context);
    fprintf(stderr, "Error creating client SSL context.\n");
    return NULL;
  }

  if (SSL_accept(client_context) <= 0)
  {
    SSL_free(client_context);
    fprintf(stderr, "Error accepting (handshake) client SSL context.\n");
    return NULL;
  }

  return client_context;
}

/* Install signal handler */
void install_signal_handler() {
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGHUP, SIG_IGN);
}

/* Signal handler of Lisod server process */
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