
/* This is a simple function to retrieve a URL and save it to the
   update directory.
*/

#include <sys/types.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>

/* We'll use snarf, since it's simpler and we have more control over the code */
/*#define USE_WGET*/
#define USE_SNARF

#ifdef USE_SNARF
#include "config.h"
#include "url.h"
#include "options.h"
#include "util.h"
#endif

#include "mkdirhier.h"
#include "log_output.h"

#define WGET            "wget"
#define UPDATE_PATH     "%s/.loki/update"


#ifdef USE_WGET
/* This was the default URL transport mechanism, but it's a little
   unwieldy because of the verboseness of the output.
*/
static int wget_url(const char *url, char *file, int maxpath,
                    int (*update)(float percentage, void *udata), void *udata)
{
    const char *home;
    const char *base;
    int pipefd[2];
    char path[PATH_MAX];
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
        log(LOG_ERROR, "Path too long for internal buffer\n");
        return(-1);
    }

    /* First create a pipe for communicating between child and parent */
    if ( pipe(pipefd) < 0 ) {
        log(LOG_ERROR, "Couldn't create IPC pipe\n");
        return(-1);
    }

    log(LOG_NORMAL, "Retrieving URL:\n%s\n", url);

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
            close(pipefd[1]);
            argc = 0;
            args[argc++] = WGET;
            args[argc++] = "-P";
            args[argc++] = path;
            args[argc++] = "-c";
            args[argc++] = url;
            args[argc] = NULL;
            execvp(args[0], args);
            fprintf(stderr, "Couldn't exec " WGET "\n");
            exit(-1);
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
        if ( (len == (sizeof(line)-1)) || (line[len] == '\n') ) {
            line[len] = '\0';

            /* Check for N% output */
            spot = strchr(line, '%');
            if ( spot ) {
                while ( (spot > line) && isdigit(*(spot-1)) ) {
                    --spot;
                }
                percentage = (float)atoi(spot);
            } else {
                /* Log the download output */
                log(LOG_NORMAL, "%s\n", line);
            }
            len = 0;
        } else {
            len += count;
        }

        /* Update the UI */
        if ( update ) {
            cancelled = update(percentage, udata);
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
    if ( status == 0 ) {
        sprintf(file, "%s/%s", path, base);
    }
    return(status);
}
#endif /* USE_WGET */

#ifdef USE_SNARF
int default_opts = 0; /* For the snarf code */

static int snarf_url(const char *url, char *file, int maxpath,
                     int (*update)(float percentage, void *udata), void *udata)
{
    const char *home;
    const char *base;
    char path[PATH_MAX];
    UrlResource *rsrc;
    int status;

    /* Get the path where files are stored */
    home = getenv("HOME");
    if ( ! home ) {
        home = "";
    }
    sprintf(path, UPDATE_PATH, home);

    /* Get the full output name */
    base = strrchr(url, '/');
    if ( base ) {
        base = base+1;
    } else {
        base = url;
    }
    strcat(path, "/");
    strcat(path, base);
    if ( maxpath < (strlen(path)+1) ) {
        log(LOG_ERROR, "Path too long for internal buffer\n");
        return(-1);
    }
    mkdirhier(path);

    log(LOG_NORMAL, "URL: %s\n", url);

    rsrc = url_resource_new();
    if ( ! rsrc ) {
        log(LOG_ERROR, "Out of memory\n");
        return(-1);
    }
    rsrc->url = url_new();
    if ( ! rsrc->url ) {
        log(LOG_ERROR, "Out of memory\n");
        url_resource_destroy(rsrc);
        return(-1);
    }
    if ( ! url_init(rsrc->url, url) ) {
        log(LOG_ERROR, "Malformed URL, aborting\n");
        url_resource_destroy(rsrc);
        return(-1);
    }
    rsrc->outfile = strdup(path);
    rsrc->outfile_offset = get_file_size(rsrc->outfile);
    if ( rsrc->outfile_offset ) {
        rsrc->options |= OPT_RESUME;
    }
    rsrc->progress = update;
    rsrc->progress_udata = udata;
    if ( transfer(rsrc) ) {
        status = 0;
    } else {
        status = -1;
    }
    strcpy(file, path);
    url_resource_destroy(rsrc);
    return(status);
}
#endif /* USE_SNARF */

int get_url(const char *url, char *file, int maxpath,
            int (*update)(float percentage, void *udata), void *udata)
{
#if defined(USE_WGET)
    return wget_url(url, file, maxpath, update, udata);
#elif defined(USE_SNARF)
    return snarf_url(url, file, maxpath, update, udata);
#else
#error No URL transport mechanism
#endif
}
