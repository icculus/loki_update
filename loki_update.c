
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "update_ui.h"
#include "setupdb.h"
#include "log_output.h"

static void print_usage(char *argv0)
{
    fprintf(stderr, "Usage: %s [product]\n", argv0);
}

static void goto_installpath(char *argv0)
{
    char temppath[PATH_MAX];
    char datapath[PATH_MAX];
    char *home;

    home = getenv("HOME");
    if ( ! home ) {
        home = ".";
    }

    strcpy(temppath, argv0);    /* If this overflows, it's your own fault :) */
    if ( ! strrchr(temppath, '/') ) {
        char *path;
        char *last;
        int found;

        found = 0;
        path = getenv("PATH");
        do {
            /* Initialize our filename variable */
            temppath[0] = '\0';

            /* Get next entry from path variable */
            last = strchr(path, ':');
            if ( ! last )
                last = path+strlen(path);

            /* Perform tilde expansion */
            if ( *path == '~' ) {
                strcpy(temppath, home);
                ++path;
            }

            /* Fill in the rest of the filename */
            if ( last > (path+1) ) {
                strncat(temppath, path, (last-path));
                strcat(temppath, "/");
            }
            strcat(temppath, "./");
            strcat(temppath, argv0);

            /* See if it exists, and update path */
            if ( access(temppath, X_OK) == 0 ) {
                ++found;
            }
            path = last+1;

        } while ( *last && !found );

    } else {
        /* Increment argv0 to the basename */
        argv0 = strrchr(argv0, '/')+1;
    }

    /* Now canonicalize it to a full pathname for the data path */
    datapath[0] = '\0';
    if ( realpath(temppath, datapath) ) {
        /* There should always be '/' in the path */
        *(strrchr(datapath, '/')) = '\0';
    }
    if ( ! *datapath || (chdir(datapath) < 0) ) {
        fprintf(stderr, "Couldn't change to install directory\n");
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    update_UI *ui;

    /* Initialize the UI */
    goto_installpath(argv[0]);
    ui = &gtk_ui;
    if ( ui->init(argc, argv) < 0 ) {
        return(3);
    }

    /* Stage 1: Update ourselves, if necessary */
    {
        switch (ui->auto_update(PRODUCT)) {
            /* An error? return an error code */
            case -1:
                ui->cleanup();
                return(255);
            /* No update needed?  Continue.. */
            case 0:
                break;
            /* Patched ourselves, restart */
            default:
                ui->cleanup();
                execvp(argv[0], argv);
                fprintf(stderr, "Couldn't exec ourselves!  Exiting\n");
                return(255);
        }
    }

    /* Stage 2: If we are being run automatically, update the product */
    if ( argv[1] ) {
        int i, status;

        i = 0;
        if ( argv[1][0] == '-' ) {
            for ( i=1; argv[i]; ++i ) {
                if ( (strcmp(argv[i], "--product_name") == 0) ||
                     (strcmp(argv[i], "--") == 0) ) {
                    break;
                }
            }
        }
        if ( ! argv[i+1] ) {
            ui->cleanup();
            print_usage(argv[0]);
            return(1);
        }
        status = 0;
        switch (ui->auto_update(argv[i+1])) {
            /* An error? return an error code */
            case -1:
                status = 3;
                break;
            /* Succeeded, or no patch needed */
            default:
                break;
        }
        ui->cleanup();

        /* Re-exec everything after "--" on command-line */
        for ( i=1; argv[i]; ++i ) {
            if ( strcmp(argv[i], "--") == 0 ) {
                ++i;
                execvp(argv[i], &argv[i]);
                fprintf(stderr, "Couldn't exec %s!  Exiting\n", argv[i]);
                return(255);
            }
        }
        return(status);
    }

    /* Stage 3: Generate a list of products and update them */
    ui->perform_updates();
    ui->cleanup();

    return(0);
}
