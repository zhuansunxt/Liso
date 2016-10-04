/******************************************************************************
 * Reference: http://www.enderunix.org/docs/eng/daemon.php                    *
 * Modified by: Wolf Richter <wolf@cs.cmu.edu>                                *
 * O_EXCL Bug Fix by: Ming Han <mteh@andrew.cmu.edu                           *
 ******************************************************************************/


/* daemonize includes */
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/***** Utility Functions *****/

/**
 * internal signal handler
 */
void signal_handler(int sig)
{
        switch(sig)
        {
                case SIGHUP:
                        /* rehash the server */
                        break;          
                case SIGTERM:
                        /* finalize and shutdown the server */
                        // TODO: liso_shutdown(NULL, EXIT_SUCCESS);
                        break;    
                default:
                        break;
                        /* unhandled signal */      
        }       
}

/** 
 * internal function daemonizing the process
 */
int daemonize(char* lock_file)
{
        /* drop to having init() as parent */
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

        lfp = open(lock_file, O_RDWR|O_CREAT, 0640);
        
        if (lfp < 0)
                exit(EXIT_FAILURE); /* can not open */

        if (lockf(lfp, F_TLOCK, 0) < 0)
                exit(EXIT_SUCCESS); /* can not lock */
        
        /* only first instance continues */
        sprintf(str, "%d\n", getpid());
        write(lfp, str, strlen(str)); /* record pid to lockfile */

        signal(SIGCHLD, SIG_IGN); /* child terminate signal */

        signal(SIGHUP, signal_handler); /* hangup signal */
        signal(SIGTERM, signal_handler); /* software termination signal from kill */

        // TODO: log --> "Successfully daemonized lisod process, pid %d."

        return EXIT_SUCCESS;
}
