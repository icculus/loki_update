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

#include "update_ui.h"
#include "patchset.h"
#include "load_products.h"
#include "load_patchset.h"
#include "url_paths.h"
#include "get_url.h"
#include "md5.h"
#include "gpg_verify.h"
#include "update.h"
#include "log_output.h"

static patchset *update_patchset;
static version_node *update_root;
static patch_path *update_path;
static patch *update_patch;
static char update_url[PATH_MAX];

/* Static variables used for this UI */
static int update_status = 0;
static patchset *product_patchset = NULL;
static int download_cancelled = 0;

/* Forward declarations for the meat of the operation */
static void download_update(void);


static void remove_update(void)
{
    if ( update_url[0] ) {
        unlink(update_url);
        update_url[0] = '\0';
    }
}

static void set_status_message(const char *message)
{
    log(LOG_STATUS, "%s\n", message);
}

static void reset_selected_update(void)
{
    update_patchset = product_patchset;
    update_path = NULL;
    update_patch = NULL;
    while ( ! update_path && update_patchset ) {
        if ( update_patchset ) {
            update_root = update_patchset->root;
        } else {
            update_root = NULL;
        }
        while ( update_root && ! update_root->selected ) {
            update_root = update_root->sibling;
        }
        if ( update_root ) {
            update_path = update_root->selected->shortest_path;
            update_patch = update_path->patch;
        } else {
            update_patchset = update_patchset->next;
        }
    }
}

static patch *skip_to_selected_update(void)
{
    if ( update_patch ) {
        while ( update_patch->installed ) {
            update_path = update_path->next;
            if ( ! update_path ) {
                do {
                    update_root = update_root->sibling;
                    if ( ! update_root ) {
                        update_patchset = update_patchset->next;
                        if ( ! update_patchset ) {
                            /* End of the line.. */
                            update_patch = NULL;
                            return(NULL);
                        }
                        update_root = update_patchset->root;
                    }
                } while ( ! update_root->selected );

                update_path = update_root->selected->shortest_path;
            }
            update_patch = update_path->patch;
        }
    }
    return(update_patch);
}

static void enable_gpg_details(const char *url, char *sig)
{
    char *file;
    char *signature;
    char *fingerprint;
    char text[1024];

    /* Fill in the information for this signature */
    file = strrchr(url, '/')+1;
    fingerprint = sig;
    signature = strchr(sig, ' ');
    *signature++ = '\0';
    sprintf(text, "%s\nSigned by %s\nGPG Fingerprint: ", file, signature);
    while ( *fingerprint ) {
        strncat(text, fingerprint, 4);
        strcat(text, " ");
        fingerprint += 4;
    }
    strcat(text, "\n");
    log(LOG_NORMAL, "%s", text);
}

static gpg_result do_gpg_verify(const char *file, char *sig, int maxsig)
{
    gpg_result gpg_code;

    set_status_message(_("Running GPG..."));
    gpg_code = gpg_verify(file, sig, maxsig, NULL, NULL);
    if ( gpg_code == GPG_NOPUBKEY ) {
        set_status_message(_("Downloading public key"));
        get_publickey(sig, NULL, NULL);
        gpg_code = gpg_verify(file, sig, maxsig, NULL, NULL);
    }
    return gpg_code;
}

static void cleanup_update(const char *status_msg, int update_obsolete)
{
    /* Remove the update patch file */
    if ( update_obsolete ) {
        remove_update();
    }

    /* Deselect the current patch path */
    select_node(update_patch->node, 0);

    /* We succeeded, update the status */
    if ( status_msg ) {
        set_status_message(status_msg);
    } else {
        /* User cancelled the update? */
        return;
    }

    /* Handle auto-update of the next set of patches, if any */
    if ( update_status >= 0 ) {
        download_update();
    }
}

static void update_product(const char *product_name)
{
    patchset *patchset;

    /* Clean up any product patchsets that may be around */
    if ( product_patchset ) {
        free_patchset(product_patchset);
        product_patchset = NULL;
    }

    /* Create a patchset for this product */
    patchset = create_patchset(product_name);
    if ( ! patchset ) {
        log(LOG_WARNING, "Unable to open product '%s'\n", product_name);
        return;
    }

    /* Reset the panel */
    set_status_message("");
    set_status_message(get_product_description(product_name));
    
    /* Download the patch list */
    strcpy(update_url, get_product_url(patchset->product_name));
    if ( get_url(update_url, update_url, sizeof(update_url), NULL,NULL) != 0 ) {
        remove_update();
        /* Tell the user what happened, and wait before continuing */
        if ( download_cancelled ) {
            set_status_message(_("Download cancelled"));
        } else {
            set_status_message(_("Unable to retrieve update list"));
        }
        free_patchset(patchset);
        return;
    }
    set_status_message(_("Retrieved update list"));
    
    /* Turn the patch list into a set of patches */
    load_patchset(patchset, update_url);
    remove_update();
    
    /* Add this patchset to our list */
    product_patchset = patchset;

    /* Skip to the first selected patchset and component */
    reset_selected_update();

    /* See if there are any updates available */
    if ( ! product_patchset || ! update_patch ) {
        /* The continue button becomes a finished button, no updates */
        set_status_message(_("No new updates available"));
    } else {
        /* Handle auto-update mode */
        download_update();
    }
}

static void download_update(void)
{
    patch *patch;
    char text[1024];
    const char *url;
    char sig[1024];
    char sum_file[PATH_MAX];
    char md5_real[CHECKSUM_SIZE+1];
    char md5_calc[CHECKSUM_SIZE+1];
    FILE *fp;
    verify_result verified;

    /* Verify that we have an update to perform */
    patch = skip_to_selected_update();
    if ( ! patch ) {
        return;
    }
    patch->installed = 1;

    /* Show the initial status for this update */
    snprintf(text, (sizeof text), "%s: %s",
             get_product_description(patch->patchset->product_name),
             patch->description);
    set_status_message(text);

    /* Download the update from the server */
    verified = DOWNLOAD_FAILED;
    do {
        /* Grab the next URL to try */
        url = get_next_url(patch->patchset->mirrors, patch->file);
        if ( ! url ) {
            break;
        }

        /* Download the update */
        set_status_message(_("Downloading update"));
        strcpy(update_url, url);
        if ( get_url(update_url, update_url, sizeof(update_url),
                     NULL, NULL) != 0 ) {
            /* The download was cancelled or the download failed */
            set_url_status(patch->patchset->mirrors, URL_FAILED);
            verified = DOWNLOAD_FAILED;
        } else {
            verified = VERIFY_UNKNOWN;
        }
    
        /* Verify the update */

        /* First check the GPG signature */
        if ( verified == VERIFY_UNKNOWN ) {
            set_status_message(_("Verifying GPG signature"));
            sprintf(sum_file, "%s.sig", url);
            if ( get_url(sum_file, sum_file, sizeof(sum_file),
                         NULL, NULL) == 0 ) {
                switch (do_gpg_verify(sum_file, sig, sizeof(sig))) {
                    case GPG_NOTINSTALLED:
                        set_status_message(_("GPG not installed"));
                        verified = VERIFY_UNKNOWN;
                        break;
                    case GPG_CANCELLED:
                        set_status_message(_("GPG was cancelled"));
                        verified = VERIFY_UNKNOWN;
                        break;
                    case GPG_NOPUBKEY:
                        set_status_message(_("GPG key not available"));
                        verified = VERIFY_UNKNOWN;
                        break;
                    case GPG_IMPORTED:
                        /* Used internally, never happens */
                        break;
                    case GPG_VERIFYFAIL:
                        set_url_status(patch->patchset->mirrors, URL_FAILED);
                        set_status_message(_("GPG verify failed"));
                        verified = VERIFY_FAILED;
                        break;
                    case GPG_VERIFYOK:
                        set_status_message(_("GPG verify succeeded"));
                        enable_gpg_details(update_url, sig);
                        verified = VERIFY_OK;
                        break;
                }
            } else {
                set_status_message(_("GPG signature not available"));
            }
            unlink(sum_file);
        }
        /* Now download the MD5 checksum file */
        if ( verified == VERIFY_UNKNOWN ) {
            set_status_message(_("Verifying MD5 checksum"));
            sprintf(sum_file, "%s.md5", url);
            if ( get_url(sum_file, sum_file, sizeof(sum_file),
                         NULL, NULL) == 0 ) {
                fp = fopen(sum_file, "r");
                if ( fp ) {
                    if ( fgets(md5_calc, sizeof(md5_calc), fp) ) {
                        set_status_message(_("Calculating MD5 checksum"));
                        md5_compute(update_url, md5_real, 0);
                        if ( strcmp(md5_calc, md5_real) != 0 ) {
                            set_url_status(patch->patchset->mirrors,URL_FAILED);
                            verified = VERIFY_FAILED;
                        }
                    }
                    fclose(fp);
                }
            } else {
                set_status_message(_("MD5 checksum not available"));
            }
            unlink(sum_file);
        }
    } while ( ((verified == DOWNLOAD_FAILED) || (verified == VERIFY_FAILED)) &&
              !download_cancelled );

    /* We either ran out of update URLs or we downloaded a valid update */
    switch (verified) {
        case VERIFY_UNKNOWN:
            set_status_message(_("Verification succeeded"));
            break;
        case VERIFY_OK:
            set_status_message(_("Verification succeeded"));
            break;
        case VERIFY_FAILED:
            set_status_message(_("Verification failed"));
            update_status = -1;
            cleanup_update(_("Update corrupted"), 1);
            return;
        case DOWNLOAD_FAILED:
            update_status = -1;
            cleanup_update(_("Unable to retrieve update"), 0);
            return;
    }

    /* Actually perform the update */
    set_status_message(_("Performing update"));
    if ( perform_update(update_url,
                        get_product_root(update_patchset->product_name),
                        NULL, NULL) != 0 ) {
        update_status = -1;
        cleanup_update(_("Update failed"), 0);
        return;
    }

    /* We're done!  A successful update! */
    ++update_status;
    cleanup_update(_("Update complete"), 1);
}

static int ttyui_detect(void)
{
    return 1;
}

static int ttyui_init(int argc, char *argv[])
{
    /* Terminal output should always be verbose */
    if ( (get_logging() > LOG_VERBOSE) && (get_logging() != LOG_NONE) ) {
        set_logging(LOG_VERBOSE);
    }

    /* Tell the patches we're applying that they should be verbose, without
       resorting to command-line hackery.
    */
    if ( get_logging() <= LOG_NORMAL ) {
        putenv("PATCH_LOGGING=1");
    }

    /* Tell the patches that we've already verified them, so don't
       perform the checksum self-check.
     */
    putenv("SETUP_NOCHECK=1");

    return 0;
}

static int ttyui_update_product(const char *product)
{
    update_status = 0;
    update_product(product);
    return update_status;
}

static int ttyui_perform_updates(const char *product)
{
    update_status = 0;
    if ( product ) {
        if ( is_valid_product(product) ) {
            update_product(product);
        } else {
            log(LOG_ERROR,
                _("%s not found, are you the one who installed it?\n"),
                product);
            update_status = -1;
        }
    } else {
        log(LOG_ERROR, _("Interactive updates require X11\n"));
        update_status = -1;
    }
    return update_status;
}

static void ttyui_cleanup(void)
{
    /* Clean up any product patchset that may be around */
    if ( product_patchset ) {
        free_patchset(product_patchset);
        product_patchset = NULL;
    }
}

update_UI tty_ui = {
    ttyui_detect,
    ttyui_init,
    ttyui_update_product,
    ttyui_perform_updates,
    ttyui_cleanup
};

#ifdef DYNAMIC_UI
update_UI *create_ui(void)
{
	update_UI *ui;

	ui = NULL;
	if ( tty_ui.detect() ) {
		ui = &tty_ui;
	}
	return ui;
}
#endif /* DYNAMIC_UI */
