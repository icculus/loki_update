
/* This is a simple function to run a patch update and update the UI */

#include <sys/types.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>

#include "log_output.h"

int perform_update(const char *update_file, const char *install_path,
    int (*update)(float percentage, int size, int total, void *udata),
                                                void *udata)
{
    int pipefd[2];
    int argc;
    const char *args[32];
    pid_t child;
    int status;
    int cancelled;
    int len, count;
    char line[1024];
    char *spot;
    float percentage;
    fd_set fdset;
    struct timeval tv;

    /* First create a pipe for communicating between child and parent */
    signal(SIGPIPE, SIG_IGN);
    if ( pipe(pipefd) < 0 ) {
        log(LOG_ERROR, "Couldn't create IPC pipe\n");
        return(-1);
    }
    chmod(update_file, 0700);

    log(LOG_VERBOSE, "Performing update:\n%s\n", update_file);

    child = fork();
    switch (child) {
        case -1:
            /* Fork failed */
            fprintf(stderr, "Couldn't fork process\n");
            return(-1);
        case 0:
            /* Child process */
            close(2);
            dup(pipefd[1]);
            close(1);
            dup(pipefd[1]);
            close(0);
            dup(pipefd[1]);
            argc = 0;
            args[argc++] = update_file;
            args[argc++] = "--nox11";
            args[argc++] = "--noreadme";
            args[argc++] = install_path;
            args[argc] = NULL;
            execv(args[0], args);
            fprintf(stderr, "Couldn't exec %s\n", args[0]);
            _exit(-1);
        default:
            break;
    }

    /* Parent, read status from child */
    cancelled = 0;
    percentage = 0.0;
    close(pipefd[1]);
    len = 0;
    while ( !cancelled ) {
        count = 0;

        /* See if there is data to read */
        FD_ZERO(&fdset);
        FD_SET(pipefd[0], &fdset);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        if ( select(pipefd[0]+1, &fdset, NULL, NULL, &tv) ) {
            count = read(pipefd[0], &line[len], 1);
            if ( count <= 0 ) {
                break;
            }
        }

        /* Parse output lines */
        if ( (len == (sizeof(line)-1)) ||
             (line[len] == '\r') || (line[len] == '\n') ) {
            line[len] = '\0';

            /* Check for N% output */
            spot = strchr(line, '%');
            if ( spot ) {
                while ( (spot > line) &&
                        (isdigit(*(spot-1)) || (*(spot-1) == '.')) ) {
                    --spot;
                }
                percentage = (float)atoi(spot);
            } else {
                /* Log the update output */
                log(LOG_VERBOSE, "%s\n", line);
            }
            len = 0;
        } else {
            len += count;
        }

        /* Update the UI */
        if ( update ) {
            cancelled = update(percentage, 0, 0, udata);
        }

        /* Why doesn't the pipe close? */
        if ( (count == 0) && (waitpid(child, &status, WNOHANG) == child) ) {
            break;
        }
    }
    if ( cancelled ) {
        kill(child, SIGTERM);
    }
    waitpid(child, &status, 0);
    close(pipefd[0]);
    if ( cancelled ) {
        status = 256;
    }
    return(status);
}
