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

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/time.h>

#include "prefpath.h"
#include "log_output.h"
#include "gpg_verify.h"

#define GPG         "gpg"
static char *keyservers[] = {
    "keyserver.lokigames.com",
    "www.keyserver.net",
    "wwwkeys.pgp.net",
    "certserver.pgp.com",
    NULL
};

gpg_result gpg_verify(const char *file, char *sig, int maxsig,
                      update_callback update, void *udata)
{
    int argc;
    char *args[9];
    pid_t child;
    int cancelled;
    int pipefd[2];
    char *prefix;
    int len, count;
    char line[1024];
    char signature[1024];
    char fingerprint[1024];
    fd_set fdset;
    struct timeval tv;
    gpg_result result;

    /* First create a pipe for communicating between child and parent */
    signal(SIGPIPE, SIG_IGN);
    if ( pipe(pipefd) < 0 ) {
        log(LOG_ERROR, _("Couldn't create IPC pipe\n"));
        return(GPG_CANCELLED);
    }

    child = fork();
    switch (child) {
        case -1:
            /* Fork failed */
            log(LOG_ERROR, _("Couldn't fork process\n"));
            return(GPG_CANCELLED);
        case 0:
            /* Child process */
            close(2);
            dup(pipefd[1]);     /* Copy the pipe to file descriptor 2 */
            close(1);
            close(0);
            close(pipefd[0]);
            close(pipefd[1]);
            argc = 0;
            args[argc++] = GPG;
            args[argc++] = "--batch";
            args[argc++] = "--logger-fd";
            args[argc++] = "1";
            args[argc++] = "--status-fd";
            args[argc++] = "2";
            args[argc++] = "--verify";
            args[argc++] = strdup(file);
            args[argc] = NULL;
            execvp(args[0], args);
            /* Uh oh, we couldn't run GPG */
            prefix = "[GNUPG:] NOTINSTALLED\n";
            write(2, prefix, strlen(prefix));
            _exit(-1);
        default:
            break;
    }

    /* Parent, read from child output (slow, but hey..) */
    result = GPG_CANCELLED;
    cancelled = 0;
    close(pipefd[1]);
    memset(sig, 0, maxsig);
    memset(signature, 0, sizeof(signature));
    memset(fingerprint, 0, sizeof(fingerprint));
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

            /* Check for GPG not installed */
            prefix = "[GNUPG:] NOTINSTALLED";
            if ( strncmp(line, prefix, strlen(prefix)) == 0 ) {
                result = GPG_NOTINSTALLED;
                break;
            }

            /* Check for needing a public key */
            prefix = "[GNUPG:] NO_PUBKEY ";
            if ( strncmp(line, prefix, strlen(prefix)) == 0 ) {
                memset(sig, 0, maxsig);
                strncpy(sig, line+strlen(prefix), maxsig);
                result = GPG_NOPUBKEY;
                break;
            }

            /* Handle signature verification fail */
            prefix = "[GNUPG:] BADSIG";
            if ( strncmp(line, prefix, strlen(prefix)) == 0 ) {
                result = GPG_VERIFYFAIL;
                break;
            }

            /* Handle signature verification succeeding */
            prefix = "[GNUPG:] GOODSIG ";
            if ( strncmp(line, prefix, strlen(prefix)) == 0 ) {
                /* Skip the initial key ID */
                prefix = line+strlen(prefix);
                while ( !isspace(*prefix) ) {
                    ++prefix;
                }
                while ( isspace(*prefix) ) {
                    ++prefix;
                }
                strncpy(signature, prefix, sizeof(signature));
                signature[strlen(prefix)] = '\0';
            }
            prefix = "[GNUPG:] VALIDSIG ";
            if ( strncmp(line, prefix, strlen(prefix)) == 0 ) {
                /* Copy the key fingerprint */
                strncpy(fingerprint, line+strlen(prefix), sizeof(fingerprint));
                prefix = fingerprint;
                while ( !isspace(*prefix) ) {
                    ++prefix;
                }
                *prefix = '\0';
                result = GPG_VERIFYOK;
                break;
            }

            len = 0;
        } else {
            len += count;
        }

        /* Update the UI */
        if ( update ) {
            cancelled = update(0, NULL, 0.0f, 0, 0, 0.0f, udata);
        }

        /* Why doesn't the pipe close? */
        if ( (count == 0) && (waitpid(child, NULL, WNOHANG) == child) ) {
            break;
        }
    }
    if ( result == GPG_VERIFYOK ) {
        snprintf(sig, maxsig, "%s %s", fingerprint, signature);
    }
    if ( cancelled ) {
        kill(child, SIGTERM);
    }
    waitpid(child, NULL, 0);
    close(pipefd[0]);
    return(result);
}

static int get_publickey_from(const char *key, const char *keyserver,
                              update_callback update, void *udata)
{
    int argc;
    char *args[16];
    pid_t child;
    int cancelled;
    int pipefd[2];
    char *prefix;
    int len, count;
    char line[1024];
    fd_set fdset;
    struct timeval tv;
    gpg_result result;

    /* First create a pipe for communicating between child and parent */
    signal(SIGPIPE, SIG_IGN);
    if ( pipe(pipefd) < 0 ) {
        log(LOG_ERROR, _("Couldn't create IPC pipe\n"));
        return(GPG_CANCELLED);
    }

    child = fork();
    switch (child) {
        case -1:
            /* Fork failed */
            update_message(LOG_ERROR, _("Couldn't fork process"), update, udata);
            return(GPG_CANCELLED);
        case 0:
            /* Child process */
            close(2);
            dup(pipefd[1]);     /* Copy the pipe to file descriptor 2 */
            close(1);
            close(0);
            close(pipefd[0]);
            close(pipefd[1]);
            argc = 0;
            args[argc++] = GPG;
            args[argc++] = "--batch";
            args[argc++] = "--logger-fd";
            args[argc++] = "1";
            args[argc++] = "--status-fd";
            args[argc++] = "2";
            args[argc++] = "--honor-http-proxy";
            args[argc++] = "--keyserver";
            args[argc++] = strdup(keyserver);
            args[argc++] = "--recv-key";
            args[argc++] = strdup(key);
            args[argc] = NULL;
            execvp(args[0], args);
            /* Uh oh, we couldn't run GPG */
            prefix = "[GNUPG:] NOTINSTALLED\n";
            write(2, prefix, strlen(prefix));
            _exit(-1);
        default:
            /* Parent, wait for child */
            break;
    }

    /* Parent, read from child output (slow, but hey..) */
    result = GPG_CANCELLED;
    cancelled = 0;
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

            /* Check for GPG not installed */
            prefix = "[GNUPG:] NOTINSTALLED";
            if ( strncmp(line, prefix, strlen(prefix)) == 0 ) {
                result = GPG_NOTINSTALLED;
                break;
            }

            /* Check for needing a public key */
            prefix = "[GNUPG:] IMPORT_RES ";
            if ( strncmp(line, prefix, strlen(prefix)) == 0 ) {
                if ( line[strlen(prefix)] == '0' ) {
                    result = GPG_NOPUBKEY;
                } else {
                    result = GPG_IMPORTED;
                }
                break;
            }

            len = 0;
        } else {
            len += count;
        }

        /* Update the UI */
        if ( update ) {
            cancelled = update(0, NULL, 0.0f, 0, 0, 0.0f, udata);
        }

        /* Why doesn't the pipe close? */
        if ( (count == 0) && (waitpid(child, NULL, WNOHANG) == child) ) {
            break;
        }
    }
    if ( cancelled ) {
        kill(child, SIGTERM);
    }
    waitpid(child, NULL, 0);
    close(pipefd[0]);
    return(result);
}

int get_publickey(const char *key, update_callback update, void *udata)
{
    FILE *fp;
    char keyserver[PATH_MAX];
    char text[1024];
    gpg_result status = GPG_CANCELLED;

    /* Open/create the list of keyservers */
    preferences_path("keyservers.txt", keyserver, sizeof(keyserver));
    fp = fopen(keyserver, "r");
    if ( ! fp ) {
        int i;

        fp = fopen(keyserver, "w+");
        if ( ! fp ) {
            sprintf(text, _("Unable to create %s"), keyserver);
            update_message(LOG_WARNING, text, update, udata);
            return(status);
        }
        for ( i=0; keyservers[i]; ++i ) {
            fprintf(fp, "%s\n", keyservers[i]);
        }
        rewind(fp);
    }

    /* Search the keyservers for the key */
    while ( fgets(keyserver, sizeof(keyserver), fp) &&
            (status != GPG_IMPORTED) ) {
        /* Trim the newline */
        keyserver[strlen(keyserver)-1] = '\0';
        if ( ! *keyserver ) {
            continue;
        }
        /* Download the public key */
        sprintf(text, _("Downloading public key %s from %s"), key, keyserver);
        update_message(LOG_VERBOSE, text, update, udata);
        status = get_publickey_from(key, keyserver, update, udata);
        switch (status) {
            case GPG_NOPUBKEY:
                update_message(LOG_VERBOSE, _("Key not found"), update, udata);
                break;
            case GPG_IMPORTED:
                update_message(LOG_VERBOSE, _("Key downloaded"), update, udata);
                break;
            default:
                break;
        }
    }
    fclose(fp);
    return(status);
}
