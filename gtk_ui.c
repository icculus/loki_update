
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include "update_ui.h"
#include "patchset.h"
#include "setupdb.h"
#include "load_patchset.h"
#include "get_url.h"
#include "md5.h"
#include "gpg_verify.h"
#include "update.h"
#include "log_output.h"

// FIXME: Add real gnu gettext() support
#define _(X)    X

#define TOPLEVEL        "loki_update"
#define UPDATE_GLADE    TOPLEVEL".glade"

static GladeXML *update_glade;
static GladeXML *readme_glade;
static GladeXML *gpg_glade;
static patchset *update_patchset;
static version_node *update_root;
static patch_path *update_path;
static patch *update_patch;
static char readme_file[PATH_MAX];
static char update_url[PATH_MAX];

/* The different notebook pages for the loki_update UI */
enum {
    PRODUCT_PAGE,
    GETLIST_PAGE,
    SELECT_PAGE,
    UPDATE_PAGE
};

/* Static variables used for this UI */
static int update_status = 0;
static int auto_update = 0;
static patchset *product_patchset = NULL;
static int update_proceeding = 0;
static int download_pending = 0;
static int download_cancelled = 0;

/* Forward declarations for the meat of the operation */
void download_update_slot( GtkWidget* w, gpointer data );
void perform_update_slot( GtkWidget* w, gpointer data );
void action_button_slot( GtkWidget* w, gpointer data );

/*********** GTK slots *************/

void main_cancel_slot( GtkWidget* w, gpointer data )
{
    gtk_main_quit();
}

void select_all_products_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *widget;
    GList *list;
    GtkWidget *button;
    const char *product_name;

    widget = glade_xml_get_widget(update_glade, "product_vbox");
    list = gtk_container_children(GTK_CONTAINER(widget));
    while ( list ) {
        button = GTK_WIDGET(list->data);
        product_name = gtk_object_get_data(GTK_OBJECT(button), "data");
        if ( strcasecmp(PRODUCT, product_name) != 0 ) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
        }
        list = list->next;
    }
}

/* Select a particular product, or deselect all of them if NULL */
static void select_product(const char *product)
{
    GtkWidget *widget;
    GList *list;
    GtkWidget *button;

    widget = glade_xml_get_widget(update_glade, "product_vbox");
    list = gtk_container_children(GTK_CONTAINER(widget));
    while ( list ) {
        button = GTK_WIDGET(list->data);
        if ( product ) {
            const char *product_name;
            product_name = gtk_object_get_data(GTK_OBJECT(button), "data");
            if ( strcasecmp(product, product_name) == 0 ) {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
                break;
            }
        } else {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
        }
        list = list->next;
    }
}

/* Deselect the currently selected product, if any */
static void deselect_product(void)
{
    GtkWidget *widget;
    GList *list;
    GtkWidget *button;

    widget = glade_xml_get_widget(update_glade, "product_vbox");
    list = gtk_container_children(GTK_CONTAINER(widget));
    while ( list ) {
        button = GTK_WIDGET(list->data);
        if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) ) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
            break;
        }
        list = list->next;
    }
}

/* Return the next selected product */
static const char *selected_product(void)
{
    GtkWidget *widget;
    GList *list;
    GtkWidget *button;

    widget = glade_xml_get_widget(update_glade, "product_vbox");
    list = gtk_container_children(GTK_CONTAINER(widget));
    while ( list ) {
        button = GTK_WIDGET(list->data);
        if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) ) {
            return gtk_object_get_data(GTK_OBJECT(button), "data");
        }
        list = list->next;
    }
    return(NULL);
}

void choose_product_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *window;
    GtkWidget *notebook;

    /* Clean up any product patchsets that may be around */
    if ( product_patchset ) {
        free_patchset(product_patchset);
        product_patchset = NULL;
    }

    /* Set the current page to the main product list page */
    notebook = glade_xml_get_widget(update_glade, "update_notebook");
    gtk_notebook_set_page(GTK_NOTEBOOK(notebook), PRODUCT_PAGE);

    /* Make sure the window is visible */
    window = glade_xml_get_widget(update_glade, TOPLEVEL);
    gtk_widget_realize(window);

    /* Clear selected products */
    select_product(NULL);
}

static void update_balls(int which, int status)
{
    GtkWidget* window;
    GtkWidget* image;
    GdkPixmap* pixmap;
    GdkBitmap* mask;
    static const char *widgets[] = {
        "list_download_pixmap",
        "update_download_pixmap",
        "update_verify_pixmap",
        "update_patch_pixmap"
    };
    static const char *images[] = {
        "pixmaps/gray_ball.xpm",
        "pixmaps/check_ball.xpm",
        "pixmaps/green_ball.xpm",
        "pixmaps/yellow_ball.xpm",
        "pixmaps/red_ball.xpm"
    };

    /* Do them all? */
    if ( which < 0 ) {
        while ( ++which < (sizeof(widgets)/sizeof(widgets[0])) ) {
            update_balls(which, status);
        }
        return;
    }

    /* Do one of the images */
    image = glade_xml_get_widget(update_glade, widgets[which]);
    window = gtk_widget_get_toplevel(image);
    pixmap = gdk_pixmap_create_from_xpm(window->window, &mask, NULL, images[status]);
    if ( pixmap ) {
        gtk_pixmap_set(GTK_PIXMAP(image), pixmap, mask);
        gdk_pixmap_unref(pixmap);
        gdk_bitmap_unref(mask);
        gtk_widget_show(image);
    }
    return;
}

static gboolean load_file( GtkText *widget, GdkFont *font, const char *file )
{
    FILE *fp;
    int pos;
    
    gtk_editable_delete_text(GTK_EDITABLE(widget), 0, -1);
    fp = fopen(file, "r");
    if ( fp ) {
        char line[BUFSIZ];
        pos = 0;
        while ( fgets(line, BUFSIZ-1, fp) ) {
            gtk_text_insert(widget, font, NULL, NULL, line, strlen(line));
        }
        fclose(fp);
    }
    gtk_editable_set_position(GTK_EDITABLE(widget), 0);

    return (fp != NULL);
}

void view_readme_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *readme;
    GtkWidget *widget;
    
    readme_glade = glade_xml_new(UPDATE_GLADE, "readme_dialog");
    glade_xml_signal_autoconnect(readme_glade);
    readme = glade_xml_get_widget(readme_glade, "readme_dialog");
    widget = glade_xml_get_widget(readme_glade, "readme_area");
    if ( readme_file[0] && readme && widget ) {
        gtk_widget_hide(readme);
        load_file(GTK_TEXT(widget), NULL, readme_file);
        gtk_widget_show(readme);
        widget = glade_xml_get_widget(update_glade, "update_readme_button");
        gtk_widget_set_sensitive(widget, FALSE);
    }
}

void close_readme_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *widget;
    
    widget = glade_xml_get_widget(readme_glade, "readme_dialog");
    gtk_widget_hide(widget);
    widget = glade_xml_get_widget(update_glade, "update_readme_button");
    gtk_widget_set_sensitive(widget, TRUE);
    gtk_object_unref(GTK_OBJECT(readme_glade));
}

void view_gpg_details_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *widget;
    
    widget = glade_xml_get_widget(gpg_glade, "gpg_dialog");
    if ( widget ) {
        gtk_widget_show(widget);
        widget = glade_xml_get_widget(update_glade, "gpg_details_button");
        if ( widget ) {
            gtk_widget_set_sensitive(widget, FALSE);
        }
    }
}

void close_gpg_details_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *widget;
    
    widget = glade_xml_get_widget(gpg_glade, "gpg_dialog");
    if ( widget ) {
        gtk_widget_hide(widget);
    }
    widget = glade_xml_get_widget(update_glade, "gpg_details_button");
    if ( widget ) {
        gtk_widget_set_sensitive(widget, TRUE);
    }
}

static void enable_gpg_details(const char *url, char *sig)
{
    GtkWidget *widget;
    char *file;
    char *signature;
    char *fingerprint;
    char text[1024];

    /* Fill in the information for this signature */
    widget = glade_xml_get_widget(gpg_glade, "gpg_details_text");
    if ( ! widget ) {
        /* Hum, not much we can do.. */
        return;
    }
    gtk_editable_delete_text(GTK_EDITABLE(widget), 0, -1);
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
    gtk_text_insert(GTK_TEXT(widget), NULL, NULL, NULL, text, strlen(text));
    gtk_editable_set_position(GTK_EDITABLE(widget), 0);

    /* Enable the button to pop up the signature details */
    widget = glade_xml_get_widget(update_glade, "gpg_details_button");
    if ( widget ) {
        gtk_widget_set_sensitive(widget, TRUE);
    }
}

void cancel_download_slot( GtkWidget* w, gpointer data )
{
    download_cancelled = 1;
}

void update_proceed_slot( GtkWidget* w, gpointer data )
{
    update_proceeding = 1;
}

static int download_update(float percentage, int size, int total, void *udata)
{
    GtkWidget *progress = (GtkWidget *)udata;

    if ( progress ) {
        gtk_progress_bar_update(GTK_PROGRESS_BAR(progress), percentage/100.0);
    }
    if ( total ) {
        GtkWidget *widget;
        char text[32], *metric;

        metric = "K";
        if ( total >= 4192 ) {
            metric = "MB";
            total /= 1024;
            size /= 1024;
        }
        widget = glade_xml_get_widget(update_glade, "size_download_label");
        if ( widget ) {
            sprintf(text, "%d %s", size, metric);
            /* Set it! */
            gtk_label_set_text(GTK_LABEL(widget), text);
        }
        widget = glade_xml_get_widget(update_glade, "total_download_label");
        if ( widget ) {
            sprintf(text, "%d %s", total, metric);
            /* Set it! */
            gtk_label_set_text(GTK_LABEL(widget), text);
        }
    }

    while( gtk_events_pending() ) {
        gtk_main_iteration();
    }
    return(download_cancelled);
}

void gtk_button_set_text(GtkButton *button, const char *text)
{
    gtk_label_set_text(GTK_LABEL(GTK_BIN(button)->child), text);
}

static void reset_selected_update(void)
{
    update_patchset = product_patchset;
    if ( update_patchset ) {
        update_root = update_patchset->root;
    } else {
        update_root = NULL;
    }
    update_path = NULL;
    update_patch = NULL;
    while ( ! update_path && update_patchset ) {
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
                } while ( update_root && ! update_root->selected );

                if ( ! update_root ) {
                    update_patchset = update_patchset->next;
                    if ( ! update_patchset ) {
                        /* End of the line.. */
                        update_patch = NULL;
                        return(NULL);
                    }
                    update_root = update_patchset->root;
                }
                update_path = update_root->selected->shortest_path;
            }
            update_patch = update_path->patch;
        }
    }
    return(update_patch);
}

static void cleanup_update(const char *status_msg)
{
    GtkWidget *status;
    GtkWidget *action;
    GtkWidget *cancel;

    /* Remove the update patch file */
    unlink(update_url);

    /* Deselect the current patch path */
    select_node(update_patch->node, 0);

    /* Handle auto-update of the next set of patches, if any */
    if ( auto_update ) {
        download_update_slot(NULL, NULL);
        return;
    }

    /* We succeeded, enable the action button, and update the status */
    action = glade_xml_get_widget(update_glade, "update_action_button");
    cancel = glade_xml_get_widget(update_glade, "update_cancel_button");
    if ( (update_status >= 0) && skip_to_selected_update() ) {
        if ( cancel ) {
            gtk_widget_set_sensitive(cancel, TRUE);
        }
        gtk_button_set_text(GTK_BUTTON(action), _("Continue"));
    } else {
        if ( cancel ) {
            gtk_widget_set_sensitive(cancel, FALSE);
        }
        gtk_button_set_text(GTK_BUTTON(action), _("Finished"));
    }
    gtk_widget_set_sensitive(action, TRUE);

    if ( status_msg ) {
        status = glade_xml_get_widget(update_glade, "update_status_label");
        if ( status ) {
            gtk_label_set_text(GTK_LABEL(status), status_msg);
        }
    } else {
        /* User cancelled the update */
        choose_product_slot(NULL, NULL);
    }
}

void action_button_slot(GtkWidget* w, gpointer data)
{
    struct stat sb;

    /* If there's a valid patch file, run it! */
    if ( stat(update_url, &sb) == 0 ) {
        perform_update_slot(NULL, NULL);
    } else {
        /* We're done - do the next patch or quit */
        if ( update_status < 0 ) {
            choose_product_slot(NULL, NULL);
        } else {
            if ( skip_to_selected_update() ) {
                download_update_slot(NULL, NULL);
            } else {
                main_cancel_slot(NULL, NULL);
            }
        }
    }
}

void cancel_button_slot(GtkWidget* w, gpointer data)
{
    if ( download_pending ) {
        cancel_download_slot(NULL, NULL);
    } else {
        cleanup_update(NULL);
    }
}

void download_update_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *notebook;
    GtkWidget *widget;
    GtkWidget *status;
    GtkWidget *verify;
    GtkWidget *action;
    GtkWidget *cancel;
    GtkWidget *progress;
    GtkWidget *verify_status;
    patch *patch;
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
    download_pending = 1;

    /* Set the current page to the product update page */
    notebook = glade_xml_get_widget(update_glade, "update_notebook");
    gtk_notebook_set_page(GTK_NOTEBOOK(notebook), UPDATE_PAGE);

    /* Set the action button to "Update" and disable it for now */
    action = glade_xml_get_widget(update_glade, "update_action_button");
    gtk_button_set_text(GTK_BUTTON(action), _("Update"));
    gtk_widget_set_sensitive(action, FALSE);

    /* Reset the panel */
    widget = glade_xml_get_widget(update_glade, "update_download_progress");
    if ( widget ) {
        gtk_progress_bar_update(GTK_PROGRESS_BAR(widget), 0.0);
    }
    widget = glade_xml_get_widget(update_glade, "update_patch_progress");
    if ( widget ) {
        gtk_progress_bar_update(GTK_PROGRESS_BAR(widget), 0.0);
    }
    widget = glade_xml_get_widget(update_glade, "update_name_label");
    if ( widget ) {
        char text[1024];
        get_product_description(patch->patchset->product, text, sizeof(text));
        strncat(text, ": ",
            ((sizeof(text)/sizeof(text[0])) - strlen(text)));
        strncat(text, patch->description,
            ((sizeof(text)/sizeof(text[0])) - strlen(text)));
        gtk_label_set_text(GTK_LABEL(widget), text);
    }
    status = glade_xml_get_widget(update_glade, "update_status_label");
    if ( status ) {
        gtk_label_set_text(GTK_LABEL(status), "");
    }
    verify = glade_xml_get_widget(update_glade, "verify_status_label");
    if ( verify ) {
        gtk_label_set_text(GTK_LABEL(verify), "");
    }
    verify_status = glade_xml_get_widget(update_glade, "gpg_status_label");
    if ( verify ) {
        gtk_label_set_text(GTK_LABEL(verify_status), "");
    }
    cancel = glade_xml_get_widget(update_glade, "update_cancel_button");
    if ( cancel ) {
        gtk_widget_set_sensitive(cancel, TRUE);
    }
    widget = glade_xml_get_widget(update_glade, "update_readme_button");
    if ( widget ) {
        gtk_widget_set_sensitive(widget, FALSE);
    }
    widget = glade_xml_get_widget(update_glade, "gpg_details_button");
    if ( widget ) {
        gtk_widget_set_sensitive(widget, FALSE);
    }
    if ( readme_file[0] ) {
        unlink(readme_file);
    }
    update_balls(-1, 0);

    /* Download the README and enable the button if we have a README */
    if ( status ) {
        gtk_label_set_text(GTK_LABEL(status), _("Downloading README"));
    }
    sprintf(readme_file, "%s.txt", patch->url);
    download_cancelled = 0;
    if ( get_url(readme_file, readme_file, sizeof(readme_file), download_update, NULL) == 0 ) {
        widget = glade_xml_get_widget(update_glade, "update_readme_button");
        if ( widget ) {
            gtk_widget_set_sensitive(widget, TRUE);
        }
    }

    /* Download the update */
    update_balls(1, 1);
    if ( status ) {
        gtk_label_set_text(GTK_LABEL(status), _("Downloading update"));
    }
    strcpy(update_url, patch->url);
    progress = glade_xml_get_widget(update_glade, "update_download_progress");
    download_cancelled = 0;
    if ( get_url(update_url, update_url, sizeof(update_url),
                  download_update, progress) != 0 ) {
        update_balls(1, 4);
        unlink(update_url);
        cleanup_update(_("Unable to retrieve update"));
        return;
    }
    update_balls(1, 2);

    /* Verify the update */
    if ( status ) {
        gtk_label_set_text(GTK_LABEL(status), _("Verifying update"));
    }
    verified = VERIFY_UNKNOWN;

    /* First check the GPG signature */
    if ( verified == VERIFY_UNKNOWN ) {
        if ( verify ) {
            gtk_label_set_text(GTK_LABEL(verify),
                               _("Downloading GPG signature"));
        }
        sprintf(sum_file, "%s.sig", patch->url);
        if ( get_url(sum_file, sum_file, sizeof(sum_file),
                     download_update, NULL) == 0 ) {
            switch (gpg_verify(sum_file, sig, sizeof(sig))) {
                case GPG_NOTINSTALLED:
                    if ( verify_status ) {
                        gtk_label_set_text(GTK_LABEL(verify_status),
                               _("GPG not installed"));
                    }
                    verified = VERIFY_UNKNOWN;
                    break;
                case GPG_CANCELLED:
                    if ( verify_status ) {
                        gtk_label_set_text(GTK_LABEL(verify_status),
                               _("GPG was cancelled"));
                    }
                    verified = VERIFY_UNKNOWN;
                    break;
                case GPG_NOPUBKEY:
                    if ( verify_status ) {
                        gtk_label_set_text(GTK_LABEL(verify_status),
                               _("GPG key not available"));
                    }
                    verified = VERIFY_UNKNOWN;
                    break;
                case GPG_VERIFYFAIL:
                    if ( verify_status ) {
                        gtk_label_set_text(GTK_LABEL(verify_status),
                               _("GPG verify failed"));
                    }
                    verified = VERIFY_FAILED;
                    break;
                case GPG_VERIFYOK:
                    if ( verify_status ) {
                        gtk_label_set_text(GTK_LABEL(verify_status),
                               _("GPG verify succeeded"));
                    }
                    enable_gpg_details(update_url, sig);
                    verified = VERIFY_OK;
                    break;
            }
        } else {
            if ( verify_status ) {
                gtk_label_set_text(GTK_LABEL(verify_status),
                               _("GPG signature not available"));
            }
        }
        unlink(sum_file);
    }
    /* Now download the MD5 checksum file */
    if ( verified == VERIFY_UNKNOWN ) {
        if ( verify ) {
            gtk_label_set_text(GTK_LABEL(verify),
                               _("Downloading MD5 checksum"));
        }
        sprintf(sum_file, "%s.md5", patch->url);
        if ( get_url(sum_file, sum_file, sizeof(sum_file),
                     download_update, NULL) == 0 ) {
            fp = fopen(sum_file, "r");
            if ( fp ) {
                if ( fgets(md5_calc, sizeof(md5_calc), fp) ) {
                    md5_compute(update_url, md5_real, 0);
                    if ( strcmp(md5_calc, md5_real) != 0 ) {
                        verified = VERIFY_FAILED;
                    }
                }
                fclose(fp);
            }
        }
        unlink(sum_file);
    }
    switch (verified) {
        case VERIFY_OK:
            if ( verify ) {
                gtk_label_set_text(GTK_LABEL(verify), _("Update okay"));
            }
            update_balls(2, 2);
            break;
        case VERIFY_UNKNOWN:
            if ( verify ) {
                gtk_label_set_text(GTK_LABEL(verify), _("Update okay"));
            }
            update_balls(2, 3);
            break;
        case VERIFY_FAILED:
            if ( verify ) {
                gtk_label_set_text(GTK_LABEL(verify), _("Update corrupted"));
            }
            update_balls(2, 4);
            unlink(update_url);
            update_status = -1;
            cleanup_update(_("Verification failed"));
            return;
    }
    if ( status ) {
        gtk_label_set_text(GTK_LABEL(status), _("Verification succeeded"));
    }
    download_pending = 0;
    gtk_widget_set_sensitive(action, TRUE);

    /* Wait for the user to confirm the update */
    if ( auto_update ) {
        perform_update_slot(NULL, NULL);
    }
}

void perform_update_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *action;
    GtkWidget *status;
    GtkWidget *cancel;
    GtkWidget *progress;

    /* Actually perform the update */
    update_balls(3, 1);
    action = glade_xml_get_widget(update_glade, "update_action_button");
    if ( action ) {
        gtk_widget_set_sensitive(action, FALSE);
    }
    status = glade_xml_get_widget(update_glade, "update_status_label");
    if ( status ) {
        gtk_label_set_text(GTK_LABEL(status), _("Performing update"));
    }
    cancel = glade_xml_get_widget(update_glade, "update_cancel_button");
    if ( cancel ) {
        gtk_widget_set_sensitive(cancel, FALSE);
    }
    progress = glade_xml_get_widget(update_glade, "update_patch_progress");
    if ( perform_update(update_url, download_update, progress) != 0 ) {
        update_balls(3, 4);
        unlink(update_url);
        update_status = -1;
        cleanup_update(_("Update failed"));
        return;
    }
    update_balls(3, 2);

    /* We're done!  A successful update! */
    ++update_status;
    cleanup_update(_("Update complete"));
}

static void empty_container(GtkWidget *widget, gpointer data)
{
    gtk_container_remove(GTK_CONTAINER(data), widget);
}

static void update_toggle_option( GtkWidget* widget, gpointer func_data)
{
    static int in_update_toggle_option = 0;
    version_node *node = (version_node *)func_data;

    if ( in_update_toggle_option ) {
        return;
    }
    in_update_toggle_option = 1;

    if ( GTK_TOGGLE_BUTTON(widget)->active ) {
        /* Select this patch path */
        select_node(node, 1);

        /* We can always enable the continue button */
        widget = glade_xml_get_widget(update_glade, "choose_continue_button");
        if ( widget ) {
            gtk_widget_set_sensitive(widget, TRUE);
        }
    } else {
        /* Deselect this patch path */
        select_node(node, 0);

        /* We may have to disable the continue button */
        widget = glade_xml_get_widget(update_glade, "choose_continue_button");
        if ( widget ) {
            patchset *patchset;
            version_node *root;
            gboolean selected = FALSE;

            for ( patchset = product_patchset;
                  patchset && !selected; patchset=patchset->next ) {
                for ( root=patchset->root;
                      root && !selected; root=root->sibling ) {
                    if ( root->selected ) {
                        selected = TRUE;
                    }
                }
            }
            gtk_widget_set_sensitive(widget, selected);
        }
    }

    /* Go through all the version nodes and set toggle state */
    for ( node = node->root; node; node = node->next ) {
        if ( node->udata ) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(node->udata),
                                         node->toggled);
        }
    }
    reset_selected_update();
    in_update_toggle_option = 0;
}

void choose_update_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *window;
    GtkWidget *notebook;
    GtkWidget *widget;
    GtkWidget *update_vbox;
    GtkWidget *frame;
    GtkWidget *vbox;
    GtkWidget *button;
    GtkWidget *progress;
    patchset *patchset;
    const char *product_name;
    char text[1024];
    char update_url[PATH_MAX];
    version_node *node, *root, *trunk;
    int selected;

    /* Clean up any product patchset that may be around */
    if ( product_patchset ) {
        free_patchset(product_patchset);
        product_patchset = NULL;
    }

    /* Set the current page to the patch choosing page */
    notebook = glade_xml_get_widget(update_glade, "update_notebook");
    gtk_notebook_set_page(GTK_NOTEBOOK(notebook), GETLIST_PAGE);

    /* Make sure the window is visible */
    window = glade_xml_get_widget(update_glade, TOPLEVEL);
    gtk_widget_realize(window);

    /* Get the update option list widget */
    update_vbox = glade_xml_get_widget(update_glade, "update_vbox");
    
    /* Clear any previous list of updates */
    gtk_container_foreach(GTK_CONTAINER(update_vbox), empty_container, update_vbox);

    /* Build the list of updates for all selected products */
    selected = 0;
    while ( (product_name = selected_product()) != NULL ) {

        /* Deselect the product so it isn't caught the next time through */
        deselect_product();

        /* Create a patchset for this product */
        patchset = create_patchset(product_name);
        if ( ! patchset ) {
            log(LOG_WARNING, "Unable to open product '%s'\n", product_name);
        }

        /* Reset the panel */
        widget = glade_xml_get_widget(update_glade, "product_label");
        if ( widget ) {
            const char *description;
            description = loki_getinfo_product(patchset->product)->description;
            gtk_label_set_text(GTK_LABEL(widget), description);
        }
        widget = glade_xml_get_widget(update_glade, "update_list_progress");
        if ( widget ) {
            gtk_progress_bar_update(GTK_PROGRESS_BAR(widget), 0.0);
        }
        widget = glade_xml_get_widget(update_glade, "list_status_label");
        if ( widget ) {
            gtk_label_set_text(GTK_LABEL(widget), "");
        }
        widget = glade_xml_get_widget(update_glade, "list_cancel_button");
        if ( widget ) {
            gtk_widget_set_sensitive(widget, TRUE);
        }
        widget = glade_xml_get_widget(update_glade, "list_done_button");
        if ( widget ) {
            gtk_widget_set_sensitive(widget, FALSE);
        }
    
        /* Download the patch list */
        update_balls(0, 1);
        strcpy(update_url, loki_getinfo_product(patchset->product)->url);
        progress = glade_xml_get_widget(update_glade, "update_list_progress");
        download_cancelled = 0;
        if ( get_url(update_url, update_url, sizeof(update_url),
                      download_update, progress) != 0 ) {
            unlink(update_url);
            update_balls(0, 4);
            if ( ! download_cancelled ) {
                /* Tell the user what happened, and wait before continuing */
                update_proceeding = 0;
                widget=glade_xml_get_widget(update_glade, "list_status_label");
                if ( widget ) {
                    gtk_label_set_text(GTK_LABEL(widget),
                                       _("Unable to retrieve update list"));
                }
                widget=glade_xml_get_widget(update_glade, "list_cancel_button");
                if ( widget ) {
                    gtk_widget_set_sensitive(widget, FALSE);
                }
                widget = glade_xml_get_widget(update_glade, "list_done_button");
                if ( widget ) {
                    gtk_widget_set_sensitive(widget, TRUE);
                }
                while ( ! update_proceeding ) {
                    gtk_main_iteration();
                }
            }
            free_patchset(patchset);
            continue;
        }
        update_balls(0, 2);
    
        /* Turn the patch list into a set of patches */
        load_patchset(patchset, update_url);
        unlink(update_url);
    
        /* If there are no patches, we're done with this product */
        if ( ! patchset->patches ) {
            free_patchset(patchset);
            continue;
        }
    
        /* Add a frame and label for this product */
        get_product_description(patchset->product, text, sizeof(text));
        frame = gtk_frame_new(text);
        gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
        gtk_box_pack_start(GTK_BOX(update_vbox), frame, FALSE, TRUE, 0);
        gtk_widget_show(frame);
        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add (GTK_CONTAINER (frame), vbox);
        gtk_widget_show(vbox);

        /* Build a list of available upgrades for each component */
        for ( root = patchset->root; root; root = root->sibling ) {
            for ( trunk = root; trunk; trunk = trunk->child ) {
                for ( node = trunk; node;
                      node = (node == root) ? NULL : node->sibling ) {
                    if ( node->invisible ) {
                        continue;
                    }
                    button = gtk_check_button_new_with_label(node->description);
                    if ( node->toggled ) {
                        ++selected;
                        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
                                                    TRUE);
                    } else {
                        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
                                                    FALSE);
                    }
                    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
                    gtk_signal_connect(GTK_OBJECT(button), "toggled",
                        GTK_SIGNAL_FUNC(update_toggle_option), (gpointer)node);
                    gtk_widget_show(button);
                    node->udata = button;
                }
            }
        }

        /* Add this patchset to our list */
        patchset->next = product_patchset;
        product_patchset = patchset;
    }

    /* Skip to the first selected patchset and component */
    reset_selected_update();

    /* Handle auto-update mode */
    if ( auto_update ) {
        if ( update_patch ) {
            download_update_slot(NULL, NULL);
        }
        return;
    }
    
    /* Switch the notebook to the appropriate page */
    if ( product_patchset ) {
        gtk_notebook_set_page(GTK_NOTEBOOK(notebook), SELECT_PAGE);
    
        /* Disable the continue button until an option is selected */
        widget = glade_xml_get_widget(update_glade, "choose_continue_button");
        if ( widget ) {
            if ( selected ) {
                gtk_widget_set_sensitive(widget, TRUE);
            } else {
                gtk_widget_set_sensitive(widget, FALSE);
            }
        }
    } else {
        choose_product_slot(NULL, NULL);
    }

    /* Allow the user to select the desired upgrades */
    return;
}

static void product_toggle_option( GtkWidget* widget, gpointer func_data)
{
    GList *list;
    gboolean enabled;

    widget = glade_xml_get_widget(update_glade, "product_vbox");
    list = gtk_container_children(GTK_CONTAINER(widget));
    enabled = FALSE;
    while ( list ) {
        if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(list->data)) ) {
            enabled = TRUE;
        }
        list = list->next;
    }
    widget = glade_xml_get_widget(update_glade, "product_continue_button");
    if ( widget ) {
        gtk_widget_set_sensitive(widget, enabled);
    }
}

static int gtkui_detect(void)
{
    return (getenv("DISPLAY") != NULL);
}

static int gtkui_init(int argc, char *argv[])
{
    GtkWidget *widget;
    GtkWidget *button;
    product_t *product;
    const char *product_name;
    char text[1024];

    gtk_init(&argc,&argv);

    /* Initialize Glade */
    glade_init();
    update_glade = glade_xml_new(UPDATE_GLADE, TOPLEVEL); 
    gpg_glade = glade_xml_new(UPDATE_GLADE, "gpg_dialog");

    /* Add all signal handlers defined in glade file */
    glade_xml_signal_autoconnect(update_glade);
    glade_xml_signal_autoconnect(gpg_glade);

    /* Fill in the list of products */
    widget = glade_xml_get_widget(update_glade, "product_vbox");
    if ( widget ) {
        for ( product_name=loki_getfirstproduct();
              product_name;
              product_name=loki_getnextproduct() ) {
            product = loki_openproduct(product_name);
            if ( ! product ) {
                log(LOG_WARNING, "Unable to open product '%s'\n", product_name);
                continue;
            }
            get_product_description(product, text, sizeof(text));
            button = gtk_check_button_new_with_label(text);
            gtk_object_set_data(GTK_OBJECT(button), "data",
                                (gpointer)product_name);
            gtk_box_pack_start(GTK_BOX(widget), button, FALSE, FALSE, 0);
            if ( strcasecmp(PRODUCT, product_name) != 0 ) {
                gtk_signal_connect(GTK_OBJECT(button), "toggled",
                    GTK_SIGNAL_FUNC(product_toggle_option), (gpointer)0);
                gtk_widget_show(button);
            }
            loki_closeproduct(product);
        }
    } else {
        log(LOG_ERROR, _("No product_vbox in glade file!\n"));
        return(-1);
    }
    widget = glade_xml_get_widget(update_glade, "product_continue_button");
    if ( widget ) {
        gtk_widget_set_sensitive(widget, FALSE);
    }
    return 0;
}

static int gtkui_update_product(const char *product)
{
    update_status = 0;
    select_product(product);
    auto_update = 1;
    choose_update_slot(NULL, NULL);
    while( gtk_events_pending() ) {
        gtk_main_iteration();
    }
    return update_status;
}

static int gtkui_perform_updates(void)
{
    update_status = 0;
    choose_product_slot(NULL, NULL);
    auto_update = 0;
    gtk_main();
    return update_status;
}

static void gtkui_cleanup(void)
{
    /* Clean up any product patchset that may be around */
    if ( product_patchset ) {
        free_patchset(product_patchset);
        product_patchset = NULL;
    }

    /* Remove any left over README file */
    if ( readme_file[0] ) {
        unlink(readme_file);
    }

    /* Clean up the Glade files */
    gtk_object_unref(GTK_OBJECT(update_glade));
    gtk_object_unref(GTK_OBJECT(gpg_glade));
}

update_UI gtk_ui = {
    gtkui_detect,
    gtkui_init,
    gtkui_update_product,
    gtkui_perform_updates,
    gtkui_cleanup
};
