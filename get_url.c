/*
    Loki_Update - A tool for updating Loki products over the Internet
    Copyright (C) 2000  Loki Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    info@lokigames.com
*/

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
#ifdef VERSION
#undef VERSION
#endif
#include "config.h"
#include "url.h"
#include "options.h"
#include "util.h"
#endif

#include "prefpath.h"
#include "log_output.h"
#include "update.h"
#include "setupdb.h"

#define WGET            "wget"
#define UPDATE_PATH     "%s/" LOKI_DIRNAME "/loki_update"

static const char *tmppath = "tmp";

#ifdef USE_WGET
/* This was the default URL transport mechanism, but it's a little
   unwieldy because of the verboseness of the output.
*/
static int wget_url(const char *url, char *file, int maxpath,
                    update_callback update, void *udata)
{
    const char *base;
    int pipefd[2];
    char path[PATH_MAX];
    char text[PATH_MAX];
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
    preferences_path(tmppath, path, sizeof(path);

    /* Get the full output name */
    base = strrchr(url, '/');
    if ( base ) {
        base = base+1;
    } else {
        base = url;
    }
    if ( maxpath < (strlen(path)+1+strlen(base)+1) ) {
        update_message(LOG_ERROR, _("Path too long for internal buffer"),
                       update, udata);
        return(-1);
    }

    /* First create a pipe for communicating between child and parent */
    if ( pipe(pipefd) < 0 ) {
        update_message(LOG_ERROR, _("Couldn't create IPC pipe"), update, udata);
        return(-1);
    }

    /* Show what URL is being downloaded */
    sprintf(text, "URL: %s", url);
    update_message(LOG_VERBOSE, text, update, udata);

    child = fork();
    switch (child) {
        case -1:
            /* Fork failed */
            update_message(LOG_ERROR, _("Couldn't fork process"), update, udata);
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
            fprintf(stderr, _("Couldn't exec %s\n"), WGET);
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
                log(LOG_DEBUG, "%s\n", line);
            }
            len = 0;
        } else {
            len += count;
        }

        /* Update the UI */
        if ( update ) {
            cancelled = update(0, NULL, percentage, 0, 0, 0.0f, udata);
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
                     update_callback update, void *udata)
{
    const char *base;
    char path[PATH_MAX];
    char text[PATH_MAX];
    UrlResource *rsrc;
    int status;

    /* Get the path where files are stored */
    preferences_path(tmppath, path, sizeof(path));
    mkdir(path, 0700);

    /* Get the full output name */
    base = strrchr(url, '/');
    if ( base ) {
        base = base+1;
    } else {
        base = url;
    }
    if ( ! *base ) {
        update_message(LOG_ERROR, _("No file specified in URL"), update, udata);
        return(-1);
    }
    strcat(path, "/");
    strcat(path, base);
    if ( maxpath < (strlen(path)+1) ) {
        update_message(LOG_ERROR, _("Path too long for internal buffer"),
                       update, udata);
        return(-1);
    }

    /* Show what URL is being downloaded */
    sprintf(text, "URL: %s", url);
    update_message(LOG_VERBOSE, text, update, udata);

    rsrc = url_resource_new();
    if ( ! rsrc ) {
        log(LOG_ERROR, _("Out of memory\n"));
        return(-1);
    }
    rsrc->url = url_new();
    if ( ! rsrc->url ) {
        log(LOG_ERROR, _("Out of memory\n"));
        url_resource_destroy(rsrc);
        return(-1);
    }
    if ( ! url_init(rsrc->url, url) ) {
        update_message(LOG_ERROR, _("Malformed URL, aborting"), update, udata);
        url_resource_destroy(rsrc);
        return(-1);
    }
    rsrc->outfile = strdup(path);
    rsrc->outfile_offset = get_file_size(rsrc->outfile);
    if ( rsrc->outfile_offset ) {
        rsrc->options |= OPT_RESUME;
    }
    if ( get_logging() == LOG_DEBUG ) {
        rsrc->options |= OPT_VERBOSE;
    }
    rsrc->progress = update;
    rsrc->progress_udata = udata;
    if ( transfer(rsrc) ) {
        status = 0;
        if ( update ) {
            update(0, NULL, 100.0, 0, 0, 0.0f, udata);
        }
    } else {
        status = -1;
    }
    strcpy(file, path);
    url_resource_destroy(rsrc);
    return(status);
}
#endif /* USE_SNARF */

int get_url(const char *url, char *file, int maxpath,
                     update_callback update, void *udata)
{
#if defined(USE_WGET)
    return wget_url(url, file, maxpath, update, udata);
#elif defined(USE_SNARF)
    return snarf_url(url, file, maxpath, update, udata);
#else
#error No URL transport mechanism
#endif
}

void set_tmppath(const char *path)
{
	tmppath = path;
}
