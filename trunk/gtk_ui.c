
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
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
static patch_path *update_path;
static patch *update_patch;
static char readme_file[PATH_MAX];

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
static int download_cancelled = 0;

/*********** GTK slots *************/

void main_cancel_slot( GtkWidget* w, gpointer data )
{
    /* Clean up any product patchset that may be around */
    if ( product_patchset ) {
        free_patchset(product_patchset);
        product_patchset = NULL;
    }
    gtk_main_quit();
}

static const char *selected_product(void)
{
    GtkWidget *widget;
    char *label = "";

    /* Get the currently selected product */
    widget = glade_xml_get_widget(update_glade, "product_menu");
    gtk_label_get(GTK_LABEL(GTK_BIN(widget)->child), &label);
    return label;
}

static void select_product(const char *product)
{
    GtkWidget *widget;
    GList *list, *itemlist;
    GtkWidget *menu;
    GtkWidget *menuitem;
    int index;

    widget = glade_xml_get_widget(update_glade, "product_menu");
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(widget));
    list = gtk_container_children(GTK_CONTAINER(menu));
    index = 0;
    while ( list ) {
        char *label = "";
        menuitem = GTK_WIDGET(list->data);
        itemlist = gtk_container_children(GTK_CONTAINER(menuitem));
        if ( itemlist ) {
            gtk_label_get(GTK_LABEL(itemlist->data), &label);
        } else {
            gtk_label_get(GTK_LABEL(GTK_BIN(widget)->child), &label);
        }
        if ( product ) {
            if ( strcasecmp(label, product) == 0 ) {
                gtk_option_menu_set_history(GTK_OPTION_MENU(widget), index);
                break;
            }
        } else {
            if ( strcasecmp(label, PRODUCT) != 0 ) {
                gtk_option_menu_set_history(GTK_OPTION_MENU(widget), index);
                break;
            }
        }
        ++index;
        list = list->next;
    }
}

static void select_next_product(void)
{
    GtkWidget *widget;
    GList *list, *itemlist;
    GtkWidget *menu;
    GtkWidget *menuitem;
    int past;
    int index;

    widget = glade_xml_get_widget(update_glade, "product_menu");
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(widget));
    list = NULL;
    past = 0;
    while ( past < 2 ) {
        if ( ! list ) {
            list = gtk_container_children(GTK_CONTAINER(menu));
            index = 0;
        }
        menuitem = GTK_WIDGET(list->data);
        itemlist = gtk_container_children(GTK_CONTAINER(menuitem));
        if ( itemlist ) {
            char *label = "";
            gtk_label_get(GTK_LABEL(itemlist->data), &label);
            if ( past && (strcasecmp(label, PRODUCT) != 0) ) {
                gtk_option_menu_set_history(GTK_OPTION_MENU(widget), index);
                break;
            }
        } else {
            ++past;
        }
        ++index;
        list = list->next;
    }
}

void choose_product_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *window;
    GtkWidget *notebook;

    /* Clean up any product patchset that may be around */
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

    /* Select the next product in the list */
    select_next_product();
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

void cancel_download_slot( GtkWidget* w, gpointer data )
{
    download_cancelled = 1;
}

void update_proceed_slot( GtkWidget* w, gpointer data )
{
    update_proceeding = 1;
}

static int download_update(float percentage, void *udata)
{
    GtkWidget *progress = (GtkWidget *)udata;

    if ( progress ) {
        gtk_progress_bar_update(GTK_PROGRESS_BAR(progress), percentage/100.0);
    }

    while( gtk_events_pending() ) {
        gtk_main_iteration();
    }
    return(download_cancelled);
}

static patch *skip_to_selected_patch(void)
{
    /* Skip to the next selected patch */
    while ( ! update_patch->selected ) {
        update_patch = update_patch->next;
        if ( ! update_patch ) {
            update_path = update_path->next;
            if ( ! update_path ) {
                /* End of the line.. */
                return(NULL);
            }
            update_patch = update_path->patches;
        }
    }
    return update_patch;
}

void gtk_button_set_text(GtkButton *button, const char *text)
{
    gtk_label_set_text(GTK_LABEL(GTK_BIN(button)->child), text);
}

static void cancel_update(const char *status)
{
    GtkWidget *widget;

    widget = glade_xml_get_widget(update_glade, "update_action_button");
    if ( widget ) {
        gtk_button_set_text(GTK_BUTTON(widget), _("Done"));
        gtk_signal_connect(GTK_OBJECT(widget), "clicked",
            GTK_SIGNAL_FUNC(choose_product_slot), (gpointer)0);
        gtk_widget_set_sensitive(widget, TRUE);
    }
    widget = glade_xml_get_widget(update_glade, "update_cancel_button");
    if ( widget ) {
        gtk_widget_set_sensitive(widget, FALSE);
    }
    widget = glade_xml_get_widget(update_glade, "update_status_label");
    if ( widget ) {
        gtk_label_set_text(GTK_LABEL(widget), status);
    }
}

void perform_update_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *notebook;
    GtkWidget *widget;
    GtkWidget *status;
    GtkWidget *verify;
    GtkWidget *action;
    GtkWidget *cancel;
    GtkWidget *progress;
    char update_url[PATH_MAX];
    char sig[1024];
    char sum_file[PATH_MAX];
    char md5_real[CHECKSUM_SIZE+1];
    char md5_calc[CHECKSUM_SIZE+1];
    FILE *fp;
    verify_result verified;

    /* Set the current page to the product update page */
    notebook = glade_xml_get_widget(update_glade, "update_notebook");
    gtk_notebook_set_page(GTK_NOTEBOOK(notebook), UPDATE_PAGE);

    /* Set the action button to "Update" and disable it for now */
    action = glade_xml_get_widget(update_glade, "update_action_button");
    if ( ! action ) {
        log(LOG_ERROR, "No 'update_action_button'");
        return;
    }
    gtk_button_set_text(GTK_BUTTON(action), _("Update"));
    gtk_signal_connect(GTK_OBJECT(action), "clicked",
        GTK_SIGNAL_FUNC(update_proceed_slot), (gpointer)0);
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
        gtk_label_set_text(GTK_LABEL(widget), update_patch->description);
    }
    status = glade_xml_get_widget(update_glade, "update_status_label");
    if ( status ) {
        gtk_label_set_text(GTK_LABEL(status), "");
    }
    verify = glade_xml_get_widget(update_glade, "verify_status_label");
    if ( verify ) {
        gtk_label_set_text(GTK_LABEL(verify), "");
    }
    cancel = glade_xml_get_widget(update_glade, "update_cancel_button");
    if ( cancel ) {
        gtk_widget_set_sensitive(cancel, TRUE);
    }
    widget = glade_xml_get_widget(update_glade, "update_readme_button");
    if ( widget ) {
        gtk_widget_set_sensitive(widget, TRUE);
    }
    if ( readme_file[0] ) {
        unlink(readme_file);
    }
    update_balls(-1, 0);

    /* Download the README and enable the button if we have a README */
    if ( status ) {
        gtk_label_set_text(GTK_LABEL(status), _("Downloading README"));
    }
    sprintf(readme_file, "%s.txt", update_patch->url);
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
    strcpy(update_url, update_patch->url);
    progress = glade_xml_get_widget(update_glade, "update_download_progress");
    download_cancelled = 0;
    if ( get_url(update_url, update_url, sizeof(update_url),
                  download_update, progress) != 0 ) {
        update_balls(1, 4);
        cancel_update(_("Unable to retrieve update"));
        unlink(update_url);
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
        sprintf(sum_file, "%s.sig", update_patch->url);
        if ( get_url(sum_file, sum_file, sizeof(sum_file),
                     download_update, NULL) == 0 ) {
            verified = gpg_verify(sum_file, sig, sizeof(sig));
        }
        unlink(sum_file);
    }
    /* Now download the MD5 checksum file */
    if ( verified == VERIFY_UNKNOWN ) {
        if ( verify ) {
            gtk_label_set_text(GTK_LABEL(verify),
                               _("Downloading MD5 checksum"));
        }
        sprintf(sum_file, "%s.md5", update_patch->url);
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
                gtk_label_set_text(GTK_LABEL(verify),
                                   _("Update okay, GPG signature verified"));
            }
            update_balls(2, 2);
            break;
        case VERIFY_UNKNOWN:
            if ( verify ) {
                gtk_label_set_text(GTK_LABEL(verify),
                                   _("Update okay, unable to check signature"));
            }
            update_balls(2, 3);
            break;
        case VERIFY_FAILED:
            if ( verify ) {
                gtk_label_set_text(GTK_LABEL(verify), _("Update corrupted"));
            }
            update_balls(2, 4);
            cancel_update(_("Verification failed"));
            unlink(update_url);
            return;
    }
    if ( status ) {
        gtk_label_set_text(GTK_LABEL(status), _("Verification succeeded"));
    }

    /* Wait for the user to confirm the update */
    if ( ! auto_update ) {
        download_cancelled = 0;
        update_proceeding = 0;
        gtk_widget_set_sensitive(action, TRUE);
        while ( ! download_cancelled && ! update_proceeding ) {
            gtk_main_iteration();
        }
        if ( download_cancelled ) {
            unlink(update_url);
            choose_product_slot(NULL, NULL);
            return;
        }
        gtk_widget_set_sensitive(action, FALSE);
    }

    /* Actually perform the update */
    update_balls(3, 1);
    if ( status ) {
        gtk_label_set_text(GTK_LABEL(status), _("Performing update"));
    }
    if ( cancel ) {
        gtk_widget_set_sensitive(cancel, FALSE);
    }
    progress = glade_xml_get_widget(update_glade, "update_patch_progress");
    download_cancelled = 0;
    if ( perform_update(update_url, download_update, progress) != 0 ) {
        update_balls(3, 4);
        cancel_update(_("Update failed"));
        unlink(update_url);
        update_status = -1;
        return;
    }
    update_balls(3, 2);
    unlink(update_url);
    ++update_status;

    /* Skip to the next patch, and install it if necessary */
    update_patch->selected = 0;
    skip_to_selected_patch();
    if ( auto_update && update_patch ) {
        perform_update_slot(NULL, NULL);
    }

    /* We succeeded, enable the action button, and update the status */
    if ( update_patch ) {
        gtk_button_set_text(GTK_BUTTON(action), _("Next"));
        gtk_signal_connect(GTK_OBJECT(action), "clicked",
            GTK_SIGNAL_FUNC(perform_update_slot), (gpointer)0);
    } else {
        gtk_button_set_text(GTK_BUTTON(action), _("Done"));
        gtk_signal_connect(GTK_OBJECT(action), "clicked",
            GTK_SIGNAL_FUNC(choose_product_slot), (gpointer)0);
    }
    gtk_widget_set_sensitive(action, TRUE);
    if ( status ) {
        gtk_label_set_text(GTK_LABEL(status), _("Update complete"));
    }
}

/* Cancel the update list stage */
static void cancel_list_update(const char *status)
{
    GtkWidget *widget;

    /* Clean up any product patchset that may be around */
    if ( product_patchset ) {
        free_patchset(product_patchset);
        product_patchset = NULL;
    }

    /* Set the status message, and enable buttons appropriately */
    if ( download_cancelled ) {
        choose_product_slot(NULL, NULL);
    } else {
        widget = glade_xml_get_widget(update_glade, "list_status_label");
        if ( widget ) {
            gtk_label_set_text(GTK_LABEL(widget), status);
        }
        widget = glade_xml_get_widget(update_glade, "list_cancel_button");
        if ( widget ) {
            gtk_widget_set_sensitive(widget, FALSE);
        }
        widget = glade_xml_get_widget(update_glade, "list_done_button");
        if ( widget ) {
            gtk_widget_set_sensitive(widget, TRUE);
        }
    }
}

static void empty_container(GtkWidget *widget, gpointer data)
{
    gtk_container_remove(GTK_CONTAINER(data), widget);
}

void update_toggle_option( GtkWidget* widget, gpointer func_data)
{
    patch_path *path;
    patch *patch;

    path = gtk_object_get_data(GTK_OBJECT(widget), "data");
    if ( GTK_TOGGLE_BUTTON(widget)->active ) {
        for ( patch=path->patches; patch; patch=patch->next ) {
            ++patch->selected;
        }
    } else {
        for ( patch=path->patches; patch; patch=patch->next ) {
            --patch->selected;
        }
    }
    widget = glade_xml_get_widget(update_glade, "choose_continue_button");
    if ( widget ) {
        gboolean selected = FALSE;

        for (path=product_patchset->paths; path && !selected; path=path->next) {
            for ( patch=path->patches; patch && !selected; patch=patch->next ) {
                if ( patch->selected ) {
                    selected = TRUE;
                }
            }
        }
        gtk_widget_set_sensitive(widget, selected);
    }
}

void choose_update_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *window;
    GtkWidget *notebook;
    GtkWidget *widget;
    GtkWidget *button;
    GtkWidget *progress;
    char update_url[PATH_MAX];
    patch_path *path;
    patch *patch;
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

    /* Reset the panel */
    widget = glade_xml_get_widget(update_glade, "product_label");
    if ( widget ) {
        gtk_label_set_text(GTK_LABEL(widget), selected_product());
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

    /* Create a patchset for this product */
    product_patchset = create_patchset(selected_product());
    if ( ! product_patchset ) {
        cancel_list_update(_("Unable to open product registry"));
        return;
    }

    /* Download the patch list */
    update_balls(0, 1);
    strcpy(update_url, loki_getinfo_product(product_patchset->product)->url);
    progress = glade_xml_get_widget(update_glade, "update_list_progress");
    download_cancelled = 0;
    if ( get_url(update_url, update_url, sizeof(update_url),
                  download_update, progress) != 0 ) {
        unlink(update_url);
        update_balls(0, 4);
        cancel_list_update(_("Unable to retrieve update list"));
        return;
    }
    update_balls(0, 2);

    /* Turn the patch list into a set of patches */
    load_patchset(product_patchset, update_url);
    unlink(update_url);

    /* If there are no patches, we're done */
    if ( ! product_patchset->paths ) {
        cancel_list_update(_("There are no new updates available"));
        return;
    }
    autoselect_patches(product_patchset);
    update_path = product_patchset->paths;
    update_patch = update_path->patches;
    skip_to_selected_patch();

    /* Handle auto-update mode */
    if ( auto_update ) {
        /* Auto-select patches and update them */
        perform_update_slot(NULL, NULL);
        return;
    }

    /* Switch the notebook to the selection page */
    gtk_notebook_set_page(GTK_NOTEBOOK(notebook), SELECT_PAGE);

    /* Get the update option list widget */
    widget = glade_xml_get_widget(update_glade, "update_vbox");

    /* Clear any previous tree of upgrades */
    gtk_container_foreach(GTK_CONTAINER(widget), empty_container, widget);

    /* Build a list of available upgrades for each component */
    path = product_patchset->paths;
    selected = 0;
    while ( path ) {
        patch = path->leaf;
        button = gtk_check_button_new_with_label(patch->description);
        if ( patch->selected ) {
            ++selected;
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
        } else {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
        }
        gtk_object_set_data(GTK_OBJECT(button), "data", path);
        gtk_box_pack_start(GTK_BOX(widget), button, FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(button), "toggled",
             GTK_SIGNAL_FUNC(update_toggle_option), (gpointer)path);
        gtk_widget_show(button);
        path = path->next;
    }

    /* Disable the continue button until an option is selected */
    widget = glade_xml_get_widget(update_glade, "choose_continue_button");
    if ( widget ) {
        if ( selected ) {
            gtk_widget_set_sensitive(widget, TRUE);
        } else {
            gtk_widget_set_sensitive(widget, FALSE);
        }
    }

    /* Allow the user to select the desired upgrades */
    return;
}

static int gtkui_detect(void)
{
    return (getenv("DISPLAY") != NULL);
}

static int gtkui_init(int argc, char *argv[])
{
    GtkWidget *widget;

    gtk_init(&argc,&argv);

    /* Initialize Glade */
    glade_init();
    update_glade = glade_xml_new(UPDATE_GLADE, TOPLEVEL); 

    /* Add all signal handlers defined in glade file */
    glade_xml_signal_autoconnect(update_glade);

    /* Fill in the list of products */
    widget = glade_xml_get_widget(update_glade, "product_menu");
    if ( widget ) {
        GtkWidget *menu;
        GtkWidget *menuitem;
        const char *product;

        menu = gtk_menu_new();
        for ( product=loki_getfirstproduct();
              product;
              product=loki_getnextproduct() ) {
            /* Create a menu item for that product */
            menuitem = gtk_menu_item_new_with_label(product);
            gtk_menu_append(GTK_MENU(menu), menuitem);

            /* Keep our product hidden, show everything else */
            if ( strcasecmp(product, PRODUCT) != 0 ) {
                gtk_widget_show(menuitem);
            }
        }
        gtk_widget_show(menu);
        gtk_option_menu_set_menu(GTK_OPTION_MENU(widget), menu);
    } else {
        //log(LOG_ERROR, _("No product menu in glade file!\n"));
        return(-1);
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
    select_product(NULL);
    gtk_main();
    return update_status;
}

static void gtkui_cleanup(void)
{
    if ( readme_file[0] ) {
        unlink(readme_file);
    }
}

update_UI gtk_ui = {
    gtkui_detect,
    gtkui_init,
    gtkui_update_product,
    gtkui_perform_updates,
    gtkui_cleanup
};
