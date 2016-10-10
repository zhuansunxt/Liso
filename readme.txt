################################################################################
# README                                                                       #
#                                                                              #
# Description: This file serves as a README and documentation for Liso Server  #
#              Current development progress: Echo server (CP1)                 #
#                                                                              #
# Authors: Xiaotong Sun (xiaotons@cs.cmu.edu)                                  #
#                                                                              #
################################################################################


[TOC-1] Table of Contents
--------------------------------------------------------------------------------

        [TOC-1] Table of Contents
        [DES-2] Description of Files
        [RUN-3] How to Run
        [DAI-4] Design and Implementation


[DES-2] Description of Files
--------------------------------------------------------------------------------

Here is a listing of all files associated with Recitation 1 and what their'
purpose is:

        README                       - Usage instruction and design documentation
        src/liso.[hc]            - Entry of Liso server (the main() function)
        src/core/handle_clients  - Core implementation on handling network I/O
        src/core/handle_request  - Core implementation on handling HTTP request
        src/utilities/*          - Helper functions, I/O utilities,
                                       error handling, and other system call wrapper
        src/parser/*             - HTTP header parsing
        src/Makefile            - kkContains rules for make


[RUN-3] How to Run
--------------------------------------------------------------------------------

Building Liso server should be very simple:

        make

Running Liso server by using the following command line:

        ./lisod <HTTP PORT> <HTTPS PORT> <log file path> <lock file path> \
                <www folder path> <cgi executable path>                   \
                <private key file path>  <certificate file path>  

        <HTTP PORT>             : port Liso listens on for HTTP reqeust
        <HTTPS PORT>            : port Liso listens on for HTTPS reqeust
        <log file path>         : file that Liso write logs to
        <lock file path>        : file that Liso locks on to achieve mutula exclusion
        <www folder path>       : www folder contains static file 
        <cgi executable path>   : CGI executable program path                   
        <private key file path> : private key file Liso uses to serve HTTPS
        <certificate file path> : certificate file uses to serve HTTPS

You can clean up the folder by:

        make clean


[DAI-4] Design and Implementation
--------------------------------------------------------------------------------

4.1 Event-based Loop

Liso is an single-threaded event-based HTTP server. It maintains a finite state machine
(FSM) for each client's lifecycle on server until the session ends. More specifically, 
every time there's an incoming client, Liso registers this client and its relevant
info to a client pool. Liso server uses select() system call in an infinite while
loop. After the selector indicates there's an upcoming event, Liso scans the client
pool, identifying event to be handled, and update client's state if necessary.

The FSM for every client looks like below:

CLIENT INIT ----> READY FOR READ ----> READY FOR WRITE ----> SESSION ENDED
                        |                       |  
                        |                       |
                        -------> WAITING CGI---->

The client pool implementation is as below:

**********************************************************************************
typedef struct {

  /* Client pool global data */
  fd_set master;              /* all descritors */
  fd_set read_fds;            /* all ready-to-read descriptors */
  fd_set write_fds;           /* all ready-to_write descriptors */
  int maxfd;                  /* maximum value of all descriptors */
  int nready;                 /* number of ready descriptors */
  int maxi;                   /* maximum index of available slot */

  /* Client specific data */
  int client_fd[FD_SETSIZE];                    /* client slots */
  dynamic_buffer * client_buffer[FD_SETSIZE];   /* client's dynamic-size buffer */
  dynamic_buffer * back_up_buffer[FD_SETSIZE];  /* store historical pending request */
  size_t received_header[FD_SETSIZE];           /* store header ending's offset */
  char should_be_close[FD_SETSIZE];             /* whether client should be closed */
  client_state state[FD_SETSIZE];               /* client's state */
  char *remote_addr[FD_SETSIZE];                /* client's remote address */

  /* SSL related */
  client_type type[FD_SETSIZE];                 /* client's type: HTTP or HTTPS */
  SSL * context[FD_SETSIZE];                    /* set if client's type is HTTPS */

  /* CGI related */
  int cgi_client[FD_SETSIZE];
} client_pool;
**********************************************************************************

4.2 Concurrency

Liso is a concurrent HTTP server. Event-based architecture provides us with more 
choice on scheudling the events and control the gradularity of the concurrency.

Firstly, to ensure the server will not be blocked from a single client, Liso will set 
all network I/O socket to be non-blocking. This implementation design lead to a non-
-blocking architecture, ensuring the fairness for all clients.

Secondly, the concurrency granularity is set to be the size of each I/O event. During 
each I/O event, Liso will only handle 4KB size of the data, either for reading and
writing.

4.3 CGI implementation

If the upcoming request is a CGI request. The client will be updated on its state to be 
'WAITING CGI'. During the time client being in this state, client will not involves in 
any network I/O, waiting for the result from the CGI process. 

To ease the communication between the network module and CGI request handling module,
Liso will construct a CGI executor pool to contain CGI executors. A CGI executor includes
information that is needed by a succesful CGI process execution.


A CGI Executor
**********************************************************************************
typedef struct CGI_executor {
  int clientfd;
  int stdin_pipe[2];    /* { write data --> stdin_pipe[1] } -> { stdin_pipe[0] --> stdin } */
  int stdout_pipe[2];   /* { read data <--  stdout_pipe[0] } <-- {stdout_pipe[1] <-- stdout } */
  dynamic_buffer* cgi_buffer;
  CGI_param* cgi_parameter;
} CGI_executor;
**********************************************************************************

4.4 Dynamic Buffer I/O Utility

Note that a dynamic buffer type is Liso specific. It is used for both network I/O and
process data communication. A dynamic buffer can be freely appended with data without
the risk of buffer overflow. The implementation is similar to C++ vector STL. When
the buffer's size reaches the capacity of the buffer, the buffer will detect this and
double the buffer capacity by allocating more memory space on the heap.

A Dynamic Buffer Type
**********************************************************************************
typedef struct dynamic_buffer{
  char *buffer;
  size_t offset;
  size_t capacity;
  size_t send_offset;
} dynamic_buffer;
**********************************************************************************














