
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>

#include "log_output.h"
#include "gpg_verify.h"

#define GPG         "gpg"
#define KEYSERVER   "certserver.pgp.com"

typedef enum {
    GPG_NOTINSTALLED,
    GPG_CANCELLED,
    GPG_NOPUBKEY,
    GPG_VERIFYFAIL,
    GPG_VERIFYOK
} gpg_result;


static gpg_result check_signature(const char *file, char *sig, int maxsig)
{
    int argc;
    const char *args[9];
    pid_t child;
    int status;
    int pipefd[2];
    char *prefix;
    int len, count;
    char line[1024];
    char signature[1024];
    char fingerprint[1024];
    gpg_result result;

    /* First create a pipe for communicating between child and parent */
    if ( pipe(pipefd) < 0 ) {
        log(LOG_ERROR, "Couldn't create IPC pipe\n");
        return(GPG_CANCELLED);
    }

    child = fork();
    switch (child) {
        case -1:
            /* Fork failed */
            log(LOG_ERROR, "Couldn't fork process\n");
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
            args[argc++] = file;
            args[argc] = NULL;
            execvp(args[0], args);
            /* Uh oh, we couldn't run GPG */
            prefix = "[GNUPG:] NOTINSTALLED\n";
            write(2, prefix, strlen(prefix));
            exit(-1);
        default:
            break;
    }

    /* Parent, read from child output (slow, but hey..) */
    result = GPG_CANCELLED;
    close(pipefd[1]);
    memset(sig, 0, maxsig);
    memset(signature, 0, sizeof(signature));
    memset(fingerprint, 0, sizeof(fingerprint));
    len = 0;
    while ( (count=read(pipefd[0], &line[len], 1)) >= 0 ) {
        if ( (len == (sizeof(line)-1)) || (line[len] == '\n') ) {
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
        /* Why doesn't the pipe close? */
        if ( (count == 0) && (waitpid(child, &status, WNOHANG) == child) ) {
            break;
        }
    }
    if ( result == GPG_VERIFYOK ) {
        snprintf(sig, maxsig, "%s %s", fingerprint, signature);
    }
    waitpid(child, &status, 0);
    close(pipefd[0]);
    return(result);
}

static int get_publickey(const char *key)
{
    int argc;
    const char *args[6];
    pid_t child;
    int status;

    child = fork();
    switch (child) {
        case -1:
            /* Fork failed */
            log(LOG_ERROR, "Couldn't fork process\n");
            return(-1);
        case 0:
            /* Child process */
            argc = 0;
            args[argc++] = GPG;
            args[argc++] = "--keyserver";
            args[argc++] = KEYSERVER;
            args[argc++] = "--recv-key";
            args[argc++] = key;
            args[argc] = NULL;
            execvp(args[0], args);
            exit(0);
        default:
            /* Parent, wait for child */
            if ( waitpid(child, &status, 0) < 0 ) {
                kill(child, SIGTERM);
                waitpid(child, &status, 0);
                return(-1);
            }
    }
    return(status);
}

/* Verify the given signature */
verify_result gpg_verify(const char *file, char *sig, int maxsig)
{
    verify_result status;
    gpg_result gpg_code;

    gpg_code = check_signature(file, sig, maxsig);
    if ( gpg_code == GPG_NOPUBKEY ) {
        get_publickey(sig);
        gpg_code = check_signature(file, sig, maxsig);
    }
    status = VERIFY_UNKNOWN;
    switch (gpg_code) {
        case GPG_NOTINSTALLED:
        case GPG_CANCELLED:
        case GPG_NOPUBKEY:
            status = VERIFY_UNKNOWN;
            break;
        case GPG_VERIFYFAIL:
            status = VERIFY_FAILED;
            break;
        case GPG_VERIFYOK:
            status = VERIFY_OK;
            break;
    }
    return status;
}
