#include "liso.h"
#include "server.h"

int main(int args, char **argv) {

  /* Check usage */
//  if (args < 8) {
//    printlog(USAGE, argv[0]);
//    exit(1);
//  }

  /* Paramters from terminal */
  int port = atoi(argv[1]);

  /* Temp parameters */
  int listenfd, newfd;
  sockaddr_in clientaddr;
  socklen_t addrlen = sizeof(sockaddr_in);
  char remoteIP[INET6_ADDRSTRLEN];
  static client_pool pool;

  /* Create server's only listener socket */
  printlog("[Main] ************Liso Echo Server*********");
  while ((listenfd = open_listenfd(port)) < 0) ;
  printlog("[Main] Successfully create listener socket %d", listenfd);

  /* Init client pool */
  init_pool(listenfd, &pool);

  while(1) {
    printlog("[Main] Selecting...");
    pool.read_fds = pool.master;
    pool.nready = select(pool.maxfd+1, &pool.read_fds, NULL, NULL, NULL);

    /* Handle exception in select, ignore all inormal cases */
    if (pool.nready <= 0) {
      if (pool.nready < 0) {
        printlog("[Main] Error select");
      }
      continue;
    }

    if (FD_ISSET(listenfd, &pool.read_fds)) {
      /* Handle new incoming connections from client */
      newfd = accept(listenfd, (sockaddr *)&clientaddr, &addrlen);

      if (newfd < 0) {
        /* Ignore exception case in accept */
        printlog("[Main] Error when accepting new client connection");
        continue;
      }

      printlog("[Main] New connection from %s on socket %d",
               inet_ntop(clientaddr.sin_family,
                         get_in_addr((sockaddr *) &clientaddr),
                         remoteIP,
                         INET6_ADDRSTRLEN),
               newfd);

      add_client_to_pool(newfd, &pool);

      if (pool.nready <= 0) continue;   /* No more readable descriptors */
    }
    handle_clients(&pool);
  }
}
