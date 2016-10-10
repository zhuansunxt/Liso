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
client_pool *p;

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
  if (daemonize() != EXIT_SUCCESS) {
    fprintf(stderr, "Daemonizing Lisod fails");
    exit(EXIT_FAILURE);
  }
#endif

  /* Global initializations */
  if (init_log() < 0) {
    fprintf(stderr, "Error when creating log");
    exit(EXIT_FAILURE);
  }
  create_folder(WWW_FOLDER, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  dump_log("HTTP port: %d", port);
  dump_log("HTTPS port: %d", https_port);
  dump_log("Log file: %s", LOCKFILE);
  dump_log("Lock file: %s", LOCKFILE);
  dump_log("WWW folder %s", WWW_FOLDER);
  dump_log("CGI script %s", CGI_scripts);
  dump_log("Private key file %s", PRIVATE_KEY_FILE);
  dump_log("Certificate file %s", CERT_FILE);

  /* HTTP network resource parameters */
  int newfd;
  sockaddr_in clientaddr;
  socklen_t addrlen = sizeof(clientaddr);
  char remoteIP[INET6_ADDRSTRLEN];

  /* SSL initialization */
  SSL_CTX *ssl_context;
  if ((ssl_context = ssl_init()) == NULL) {
    console_log("[Main][Error] SSL init fails");
    tear_down();
    exit(EXIT_FAILURE);
  }

  /* Create server's only listener socket */
  if ((listenfd = open_listenfd(port)) < 0) {
    dump_log("Can not create HTTP port");
  }
  if ((ssl_socket = open_ssl_socket(https_port, ssl_context)) < 0) {
    dump_log("Can not create HTTPS port");
  }

  /* Init client pool */
  init_pool();
  init_CGI_pool();

  console_log("[INFO] ************Liso Echo Server*********");
  while(1) {
    p->read_fds = p->master;
    p->write_fds = p->master;
    p->nready = select(p->maxfd+1, &(p->read_fds), &(p->write_fds), NULL, NULL);

    /* Handle exception in select, ignore all inormal cases */
    if (p->nready <= 0) {
      if (p->nready < 0) {
        tear_down();
        err_sys("error in select");
      }
      continue;
    }

    if (FD_ISSET(listenfd, &(p->read_fds))) {
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
      if (p->nready <= 0) continue;   /* No more readable descriptors */
    }

    if (FD_ISSET(ssl_socket, &(p->read_fds))) {
      /* Handle new  incomming connecitions from https port */
      console_log("[Main] Incoming HTTPS client!");
      newfd = accept(ssl_socket, (sockaddr *) &clientaddr, &addrlen);
      if (newfd < 0) {
        console_log("error when accepting new client connection from HTTPS port %d", https_port);
        dump_log("[ERROR] error when accepting new client connection from HTTP port %d", https_port);
        continue;
      }

      /* wrap socket with ssl */
      SSL *client_context;
      if ((client_context = SSL_new(ssl_context)) == NULL) {
        close(ssl_socket);
        SSL_CTX_free(ssl_context);
        console_log("[SSL error] Error creating client SSL context");
        exit(EXIT_FAILURE);
      }
      if (SSL_set_fd(client_context, newfd) == 0) {
        close(ssl_socket);
        SSL_free(client_context);
        SSL_CTX_free(ssl_context);
        console_log("[SSL error] Error creating client SSL context in SSL_set_fd");
        exit(EXIT_FAILURE);
      }
      int rv;
      if ((rv = SSL_accept(client_context)) <= 0) {
        console_log("[OpenSSL Error] %s", ERR_error_string(SSL_get_error(client_context, rv), NULL));
        close(ssl_socket);
        SSL_free(client_context);
        SSL_CTX_free(ssl_context);
        console_log("[SSL error] Error accepting (handshake) client SSL contenxt)");
        exit(EXIT_FAILURE);
      }

      char *remote_addr = (char*)inet_ntop(clientaddr.sin_family,
                                    get_in_addr((sockaddr *) &clientaddr),
                                    remoteIP,
                                    INET6_ADDRSTRLEN);
      add_client_to_pool(newfd, client_context, HTTPS_CLIENT, remote_addr);
      if (p->nready <= 0) continue;   /*No more readable descriptors */
    }

    /* Handle client request : read and write */
    handle_clients();
  }
}

int daemonize() {
  int i, lfp, pid = fork();
  char str[256] = {0};
  if (pid < 0) exit(EXIT_FAILURE);
  if (pid > 0) exit(EXIT_SUCCESS);

  setsid();

  for (i = getdtablesize(); i>=0; i--)
    close(i);

  i = open("/dev/null", O_RDWR);
  dup(i); /* stdout */
  dup(i); /* stderr */
  umask(027);

  lfp = open(LOCKFILE, O_RDWR|O_CREAT, 0640);

  if (lfp < 0)
    exit(EXIT_FAILURE); /* can not open */

  if (lockf(lfp, F_TLOCK, 0) < 0)
    exit(EXIT_SUCCESS); /* can not lock */

  /* only first instance continues */
  sprintf(str, "%d\n", getpid());
  write(lfp, str, strlen(str)); /* record pid to lockfile */

  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGHUP, SIG_IGN);
  signal(SIGCHLD, SIG_IGN); /* child terminate signal */

  return EXIT_SUCCESS;

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
/*
SSL *wrap_socket_with_ssl(int client_sock, SSL_CTX *ssl_context) {
  SSL *client_context;
  int rv, err;

  console_log("New client socket: %d", client_sock);
  if ((client_context = SSL_new(ssl_context)) == NULL)
  {
    console_log("\"Error creating client SSL context");
    return NULL;
  }

  if ((rv = SSL_set_fd(client_context, client_sock)) == 0)
  {
    SSL_free(client_context);
    console_log("Error creating client SSL context");
    return NULL;
  }

  if ((rv = SSL_accept(client_context)) <= 0)
  {
    SSL_free(client_context);
    console_log("Error accepting (handshake) client SSL context");
    console_log("[OpenSSL Error] %s", ERR_error_string(SSL_get_error(client_context, rv), NULL));
    return NULL;
  }

  return client_context;
}
*/

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