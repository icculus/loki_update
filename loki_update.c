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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <locale.h>

#include "update_ui.h"
#include "log_output.h"
#include "url_paths.h"
#include "meta_url.h"
#include "get_url.h"
#include "load_products.h"


#define PACKAGE "loki_update"

static void print_usage(char *argv0)
{
    fprintf(stderr,
_("Loki Update Tool %s\n"
  "Usage: %s [options] [product or install directory]\n"
  "The options can be any of:\n"
  "    --verbose               Print verbose messages to standard output\n"
  "    --noselfcheck           Skip check for updates for the update tool\n"
  "    --tmppath PATH          Use PATH as the temporary download path\n"
  "    --update_url URL        Use URL as the list of product updates\n"),
            VERSION, argv0);
}

static void init_locale(void)
{
	setlocale (LC_ALL, "");
	bindtextdomain (PACKAGE, "locale");
	textdomain (PACKAGE);
}

static void goto_installpath(char *argv0)
{
    char temppath[PATH_MAX];
    char datapath[PATH_MAX];
    char *home;

    /* First save the original working directory (for file loading, etc.) */
    strcpy(temppath, ".");
    getcwd(temppath, sizeof(temppath));
    set_working_path(temppath);
    { static char env[PATH_MAX];
      sprintf(env, "UPDATE_CWD=%s", temppath);
      putenv(env);
    }

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
        fprintf(stderr, _("Couldn't change to install directory\n"));
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    static update_UI *ui_list[] = { &gtk_ui, &tty_ui, NULL };
    int self_check;
    int auto_update;
    const char *product;
    const char *product_path;
    const char *tmppath;
    const char *meta_url;
    const char *update_url;
    int i;
    update_UI *ui;

    /* Seed the random number generator for choosing URLs */
    srand(time(NULL));

    /* Init the i18n */
    init_locale();

    /* Parse the command line */
    self_check = 1;
    auto_update = 0;
    product = NULL;
    product_path = NULL;
    tmppath = NULL;
    meta_url = NULL;
    update_url = NULL;
    for ( i=1; argv[i] && (argv[i][0] == '-'); ++i ) {
        if ( strcmp(argv[i], "--") == 0 ) {
            break;
        }
        if ( (strcmp(argv[i], "--help") == 0) ||
             (strcmp(argv[i], "-h") == 0) ) {
            print_usage(argv[0]);
            return(0);
        } else
        if ( (strcmp(argv[i], "--version") == 0) ||
             (strcmp(argv[i], "-V") == 0) ) {
            printf("Loki Update Tool " VERSION "\n");
            return(0);
        } else
        if ( (strcmp(argv[i], "--debug") == 0) ||
             (strcmp(argv[i], "-d") == 0) ) {
            set_logging(LOG_DEBUG);
        } else
        if ( (strcmp(argv[i], "--verbose") == 0) ||
             (strcmp(argv[i], "-v") == 0) ) {
            set_logging(LOG_VERBOSE);
        } else
        if ( strcmp(argv[i], "--noselfcheck") == 0 ) {
            self_check = 0;
        } else
        if ( strcmp(argv[i], "--tmppath") == 0 ) {
            if ( ! argv[i+1] ) {
                print_usage(argv[0]);
                return(1);
            }
            tmppath = argv[++i];
        } else
        if ( strcmp(argv[i], "--meta_url") == 0 ) {
            if ( ! argv[i+1] ) {
                print_usage(argv[0]);
                return(1);
            }
            meta_url = argv[++i];
        } else
        if ( strcmp(argv[i], "--update_url") == 0 ) {
            if ( ! argv[i+1] ) {
                print_usage(argv[0]);
                return(1);
            }
            update_url = argv[++i];
        } else
        if ( strcmp(argv[i], "--product_name") == 0 ) {
            if ( ! argv[i+1] ) {
                print_usage(argv[0]);
                return(1);
            }
            /* This is being called as an auto-update from loki_utils */
            auto_update = 1;
            product = argv[++i];
        } else
        if ( strcmp(argv[i], "--product_path") == 0 ) {
            if ( ! argv[i+1] ) {
                print_usage(argv[0]);
                return(1);
            }
            product_path = argv[++i];
        }
    }
    if ( !product && argv[i] && (argv[i][0] != '-') ) {
        product = argv[i];
    }

    /* If the product is a directory, see if it contains a .manifest
       that we can write to, and if so, add it to our list of products.
     */
    if ( is_product_path(product) ) {
        const char *new_product;
        new_product = link_product_path(product);
        if ( ! new_product ) {
            fprintf(stderr, _("Couldn't modify or link to %s\n"), product);
            exit(1);
        }
        product = new_product;
    }

    /* Set correct run directory and scan for installed products */
    goto_installpath(argv[0]);
    load_product_list(product);
    if ( product_path ) {
        if ( ! product ) {
            log(LOG_ERROR, _("Install path set, but no product specified\n"));
            return(1);
        }
        set_product_root(product, product_path);
    }
    if ( tmppath ) {
        set_tmppath(tmppath);
    }
    if ( meta_url ) {
        load_meta_url(meta_url);
    }
    set_override_url(update_url);

    /* Initialize the UI */
    ui = NULL;
    for ( i=0; ui_list[i] && !ui; ++i ) {
        if ( ui_list[i]->detect() ) {
            ui = ui_list[i];
        }
    }
    if ( ui ) {
        if ( ui->init(argc, argv) < 0 ) {
            return(3);
        }
    } else {
        log(LOG_ERROR, _("No user interface modules available\n"));
        return(2);
    }

    /* Stage 1: Update ourselves, if possible */
    if ( self_check && (access(".", W_OK) == 0) && is_valid_product(PRODUCT) ) {
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
                chdir(get_working_path());
                execvp(argv[0], argv);
                fprintf(stderr, _("Couldn't exec ourselves!  Exiting\n"));
                return(255);
        }
    }

    /* Stage 2: If we are being run automatically, update the product */
    if ( product ) {
        int status;

        if ( auto_update ) {
            if ( product && ! is_valid_product(PRODUCT) ) {
                log(LOG_ERROR,
                    _("%s not found, are you the one who installed it?\n"),
                    product);
                return(1);
            }
            status = ui->auto_update(product);
        } else {
            status = ui->perform_updates(product);
        }
        switch (status) {
            /* An error? return an error code */
            case -1:
                status = 3;
                break;
            /* Succeeded, or no patch needed */
            default:
                status = 0;
                break;
        }
        ui->cleanup();

        /* Re-exec everything after "--" on command-line */
        for ( i=1; argv[i]; ++i ) {
            if ( strcmp(argv[i], "--") == 0 ) {
                ++i;
                chdir(get_working_path());
                execvp(argv[i], &argv[i]);
                fprintf(stderr, _("Couldn't exec %s!  Exiting\n"), argv[i]);
                return(255);
            }
        }
        return(status);
    }

    /* Stage 3: Generate a list of products and update them */
    ui->perform_updates(NULL);
    ui->cleanup();

    return(0);
}
