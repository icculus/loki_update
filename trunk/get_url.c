
/* This is a simple function to retrieve a URL and save it to the
   update directory.
*/

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <signal.h>

#include "mkdirhier.h"
#include "log_output.h"

#define WGET            "wget"
#define UPDATE_PATH     "%s/.loki/update"

static void take_note(int sig)
{
    return;
}

int get_url(const char *url, char *file, int maxpath)
{
    const char *home;
    const char *base;
    char path[PATH_MAX];
    int argc;
    const char *args[32];
    pid_t child;
    int status;

    /* Get the path where files are stored */
    home = getenv("HOME");
    if ( ! home ) {
        home = "";
    }
    sprintf(path, UPDATE_PATH, home);
    mkdirhier(path);

    /* Get the full output name */
    base = strrchr(url, '/');
    if ( base ) {
        base = base+1;
    } else {
        base = url;
    }
    if ( maxpath < (strlen(path)+1+strlen(base)+1) ) {
        fprintf(stderr, "Path too long for internal buffer\n");
        return(-1);
    }

#if 0
    log(LOG_NORMAL, "Retrieving URL:\n%s\n", url);
#endif
    child = fork();
    switch (child) {
        case -1:
            /* Fork failed */
            fprintf(stderr, "Couldn't fork process\n");
            return(-1);
        case 0:
            /* Child process */
            argc = 0;
            args[argc++] = WGET;
            args[argc++] = "-P";
            args[argc++] = path;
            args[argc++] = "-c";
            args[argc++] = "-nv";
            args[argc++] = url;
            args[argc] = NULL;
            execvp(args[0], args);
            fprintf(stderr, "Couldn't exec " WGET "\n");
            exit(-1);
        default:
            /* Parent, wait for child */
            signal(SIGINT, take_note);
            if ( waitpid(child, &status, 0) < 0 ) {
                signal(SIGINT, SIG_DFL);
                log(LOG_NORMAL, "Cancelling url retrieval\n");
                kill(child, SIGTERM);
                waitpid(child, &status, 0);
                return(-1);
            }
            signal(SIGINT, SIG_DFL);
            if ( status == 0 ) {
                sprintf(file, "%s/%s", path, base);
            }
    }
    return(status);
}
