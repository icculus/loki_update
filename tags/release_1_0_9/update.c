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

#include "safe_malloc.h"
#include "log_output.h"
#include "update.h"

void update_message(int level, const char *message,
                    update_callback update, void *udata)
{
    if ( update ) {
        if ( level == LOG_STATUS ) {
            update(level, message, 0.0f, 0, 0, 0.0f, udata);
        } else {
            char *text;

            text = (char *)safe_malloc(strlen(message)+2);
            sprintf(text, "%s\n", message);
            update(level, text, 0.0f, 0, 0, 0.0f, udata);
            free(text);
        }
    } else {
        log(level, "%s\n", message);
    }
}

int perform_update(const char *update_file, const char *install_path,
                   update_callback update, void *udata)
{
    char text[PATH_MAX];
    int pipefd[2];
    int argc;
    const char *args[32];
    pid_t child;
    int status;
    int cancelled;
    int len, count;
    char line[1024];
    char *spot;
    int status_updated;
    float percentage;
    fd_set fdset;
    struct timeval tv;

    /* First create a pipe for communicating between child and parent */
    signal(SIGPIPE, SIG_IGN);
    if ( pipe(pipefd) < 0 ) {
        update_message(LOG_ERROR, _("Couldn't create IPC pipe"), update, udata);
        return(-1);
    }
    chmod(update_file, 0700);

    /* Show what update file is being executed */
    sprintf(text, _("Update: %s"), update_file);
    update_message(LOG_VERBOSE, text, update, udata);
    update_message(LOG_STATUS, _("Unpacking archive"), update, udata);
    status_updated = 0;

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
            dup(pipefd[1]);
            argc = 0;
            args[argc++] = update_file;
            args[argc++] = "--nox11";
            args[argc++] = install_path;
            args[argc] = NULL;
            execv(args[0], args);
            fprintf(stderr, _("Couldn't exec %s\n"), args[0]);
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
                if ( ! status_updated ) {
                    update_message(LOG_STATUS, _("Updating files"), update, udata);
                    status_updated = 1;
                }
            } else {
                /* Log the update output */
                if ( strncmp(line, "ERROR: ", 7) == 0 ) {
                    update_message(LOG_ERROR, line+7, update, udata);
                } else
                if ( strncmp(line, "WARNING: ", 8) == 0 ) {
                    update_message(LOG_WARNING, line+8, update, udata);
                } else {
                    update_message(LOG_VERBOSE, line, update, udata);
                }
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
    return(status);
}
