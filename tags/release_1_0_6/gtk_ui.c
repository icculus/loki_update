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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

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
#include "safe_malloc.h"

#define TOPLEVEL        "loki_update"
#define UPDATE_GLADE    TOPLEVEL".glade"

static GladeXML *update_glade;
static GladeXML *readme_glade = NULL;
static GladeXML *mirrors_glade = NULL;
static GladeXML *gpg_glade = NULL;
static GladeXML *details_glade = NULL;
static GladeXML *save_glade = NULL;
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
static int one_product = 0;
static enum {
	FULLY_AUTOMATIC,
	SEMI_AUTOMATIC,
	FULLY_INTERACTIVE
} interactive = FULLY_INTERACTIVE;
static patchset *product_patchset = NULL;
static int update_proceeding = 0;
static int download_pending = 0;
static int switch_mirror = 0;
static int download_cancelled = 0;

struct download_update_info
{
    GtkWidget *status;
    GtkWidget *progress;
    GtkWidget *rate_label;
    GtkWidget *eta_label;
    float last_rate;
    time_t last_update;
};
#define MAX_RATE_CHANGE 50.0f

/* Forward declarations for the meat of the operation */
void download_update_slot( GtkWidget* w, gpointer data );
void perform_update_slot( GtkWidget* w, gpointer data );
void action_button_slot( GtkWidget* w, gpointer data );

/* Extra GTk utility functions */

void gtk_empty_container(GtkWidget *widget, gpointer data)
{
    gtk_container_remove(GTK_CONTAINER(data), widget);
}

void gtk_button_set_text(GtkButton *button, const char *text)
{
    gtk_label_set_text(GTK_LABEL(GTK_BIN(button)->child), text);
}

void gtk_button_set_sensitive(GtkWidget *button, gboolean sensitive)
{
    gtk_widget_set_sensitive(button, sensitive);

    /* Simulate a mouse crossing event, to enable button */
    if ( sensitive ) {
        int x, y;
        gboolean retval;
        GdkEventCrossing crossing;

        gtk_widget_get_pointer(button, &x, &y);
        if ( (x >= 0) && (y >= 0) &&
             (x <= button->allocation.width) &&
             (y <= button->allocation.height) ) {
                memset(&crossing, 0, sizeof(crossing));
                crossing.type = GDK_ENTER_NOTIFY;
                crossing.window = button->window;
                crossing.detail = GDK_NOTIFY_VIRTUAL;
                gtk_signal_emit_by_name(GTK_OBJECT(button),
                                        "enter_notify_event",
                                        &crossing, &retval);
        }
    }
}

struct image {
    const char *file;
    GdkPixmap *pixmap;
    GdkBitmap *bitmap;
};
struct image balls[] = {
    { "pixmaps/gray_ball.xpm",      NULL, NULL },
    { "pixmaps/green_ball.xpm",     NULL, NULL },
    { "pixmaps/check_ball.xpm",     NULL, NULL },
    { "pixmaps/yellow_ball.xpm",    NULL, NULL },
    { "pixmaps/red_ball.xpm",       NULL, NULL }
};
struct image arrows[] = {
    { "pixmaps/gray_arrow.xpm",     NULL, NULL },
    { "pixmaps/green_arrow.xpm",    NULL, NULL },
    { "pixmaps/yellow_arrow.xpm",   NULL, NULL },
    { "pixmaps/red_arrow.xpm",      NULL, NULL }
};

static void load_image(GtkWidget *window, struct image *image)
{
    if ( ! image->pixmap ) {
        image->pixmap = gdk_pixmap_create_from_xpm(window->window,
                                &image->bitmap, NULL, image->file);
    }
}
static void load_images(GtkWidget *window)
{
    int i;

    for ( i=0; i<(sizeof(balls)/sizeof(balls[0])); ++i ) {
        load_image(window, &balls[i]);
    }
    for ( i=0; i<(sizeof(arrows)/sizeof(arrows[0])); ++i ) {
        load_image(window, &arrows[i]);
    }
}

static void free_image(struct image *image)
{
    if ( image->pixmap ) {
        gdk_pixmap_unref(image->pixmap);
        image->pixmap = NULL;
    }
    if ( image->bitmap ) {
        gdk_bitmap_unref(image->bitmap);
        image->bitmap = NULL;
    }
}
static void free_images(void)
{
    int i;

    for ( i=0; i<(sizeof(balls)/sizeof(balls[0])); ++i ) {
        free_image(&balls[i]);
    }
    for ( i=0; i<(sizeof(arrows)/sizeof(arrows[0])); ++i ) {
        free_image(&arrows[i]);
    }
}

static void remove_readme(void)
{
    GtkWidget *widget;

    if ( readme_file[0] ) {
        unlink(readme_file);
        readme_file[0] = '\0';
    }
    widget = glade_xml_get_widget(update_glade, "update_readme_button");
    if ( widget ) {
        gtk_button_set_sensitive(widget, FALSE);
    }
}
static void remove_update(void)
{
    if ( update_url[0] ) {
        unlink(update_url);
        update_url[0] = '\0';
    }
}

static char *quickupdate_file(char *path, int maxlen)
{
    snprintf(path, maxlen, "%s/.loki", getenv("HOME"));
    mkdir(path, 0700);
    strncat(path, "/loki_update", maxlen-strlen(path));
    mkdir(path, 0700);
    strncat(path, "/quick-update.txt", maxlen-strlen(path));
    return(path);
}

static void save_interactive(int value)
{
    char path[PATH_MAX];
    FILE *fp;

    fp = fopen(quickupdate_file(path, sizeof(path)), "w");
    if ( fp ) {
        fprintf(fp, "%d\n", value);
        fclose(fp);
    }
}

static int load_interactive(void)
{
    int value;
    char path[PATH_MAX];
    FILE *fp;

    value = SEMI_AUTOMATIC;
    fp = fopen(quickupdate_file(path, sizeof(path)), "r");
    if ( fp ) {
        if ( fgets(path, sizeof(path), fp) ) {
            path[strlen(path)-1] = '\0';
            value = atoi(path);
        }
        fclose(fp);
    }
    return(value);
}

/*********** GTK slots *************/

void main_cancel_slot( GtkWidget* w, gpointer data )
{
    download_cancelled = 1;
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
        if ( product_name && strcasecmp(PRODUCT, product_name) != 0 ) {
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
            if ( GTK_IS_TOGGLE_BUTTON(button) ) {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
            }
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
    load_images(window);

    /* Clear selected products */
    select_product(NULL);
}

void main_menu_slot( GtkWidget* w, gpointer data )
{
    if ( one_product ) {
        main_cancel_slot(NULL, NULL);
    } else {
        choose_product_slot(NULL, NULL);
    }
}

static void update_arrows(int which, int status)
{
    GtkWidget* image;
    static const char *widgets[] = {
        "list_status_arrow",
        "update_status_arrow",
    };
    struct image *arrow;

    /* Do one of the images */
    arrow = &arrows[status];
    image = glade_xml_get_widget(update_glade, widgets[which]);
    if ( image && arrow->pixmap ) {
        gtk_pixmap_set(GTK_PIXMAP(image), arrow->pixmap, arrow->bitmap);
        gtk_widget_show(image);
    }
}

static struct flash {
    int id;
    int which;
    int color;
    int colored;
} flash_data;

static gint flash_arrow(gpointer data)
{
    struct flash *flash = (struct flash *)data;

    if ( flash->colored ) {
        update_arrows(flash->which, flash->color);
        flash->colored = 0;
    } else {
        update_arrows(flash->which, 0);
        flash->colored = 1;
    }
    return flash->id;
}

static void start_flash(int which, int status)
{
    update_arrows(which, 0);
    flash_data.which = which;
    flash_data.color = status;
    flash_data.colored = 0;
    flash_data.id = gtk_timeout_add(500, flash_arrow, &flash_data);
}

static void stop_flash(void)
{
    if ( flash_data.id ) {
        gtk_timeout_remove(flash_data.id);
        flash_data.id = 0;
    }
}

static void update_balls(int which, int status)
{
    GtkWidget* image;
    static const char *widgets[] = {
        "list_download_pixmap",
        "update_download_pixmap",
        "update_verify_pixmap",
        "update_patch_pixmap"
    };
    struct image *ball;

    /* Do them all? */
    if ( which < 0 ) {
        while ( ++which < (sizeof(widgets)/sizeof(widgets[0])) ) {
            update_balls(which, status);
        }
        return;
    }

    /* Do one of the images */
    ball = &balls[status];
    image = glade_xml_get_widget(update_glade, widgets[which]);
    if ( image && ball->pixmap ) {
        gtk_pixmap_set(GTK_PIXMAP(image), ball->pixmap, ball->bitmap);
        gtk_widget_show(image);
    }
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
        gtk_button_set_sensitive(widget, FALSE);
    }
}

void close_readme_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *widget;
    
    if ( readme_glade ) {
        widget = glade_xml_get_widget(readme_glade, "readme_dialog");
        if ( widget ) {
            gtk_widget_hide(widget);
        }
        widget = glade_xml_get_widget(update_glade, "update_readme_button");
        if ( widget ) {
            gtk_button_set_sensitive(widget, TRUE);
        }
        gtk_object_unref(GTK_OBJECT(readme_glade));
        readme_glade = NULL;
    }
}

static void clear_details_text(void)
{
    GtkWidget *widget;

    widget = glade_xml_get_widget(details_glade, "update_details_text");
    if ( widget ) {
        gtk_editable_delete_text(GTK_EDITABLE(widget), 0, -1);
    }
}

static void add_details_text(int level, const char *text)
{
    GtkWidget *widget;

    widget = glade_xml_get_widget(details_glade, "update_details_text");
    if ( widget ) {
        switch (level) {
            case LOG_ERROR:
                gtk_text_insert(GTK_TEXT(widget), NULL, NULL, NULL,
                                "ERROR: ", strlen("ERROR: "));
                break;
            case LOG_WARNING:
                gtk_text_insert(GTK_TEXT(widget), NULL, NULL, NULL,
                                "WARNING: ", strlen("WARNING: "));
                break;
            default:
                break;
        }
        if ( level > LOG_DEBUG ) {
            gtk_text_insert(GTK_TEXT(widget), NULL, NULL, NULL,
                            text, strlen(text));
        }
    }
    log(level, "%s", text);
}

static void open_save_details(void)
{
    GtkWidget *widget;
    
    save_glade = glade_xml_new(UPDATE_GLADE, "save_details_dialog");
    glade_xml_signal_autoconnect(save_glade);
    widget = glade_xml_get_widget(save_glade, "save_details_dialog");
    if ( widget ) {
        char path[PATH_MAX];

        /* Set the initial working directory and show the dialog */
        sprintf(path, "%s/", get_working_path());
        gtk_file_selection_set_filename(GTK_FILE_SELECTION(widget), path);
        gtk_widget_show(widget);

        widget = glade_xml_get_widget(details_glade, "save_details_button");
        if ( widget ) {
            gtk_button_set_sensitive(widget, FALSE);
        }
    }
}

static void close_save_details(void)
{
    GtkWidget *widget;

    if ( save_glade ) {
        widget = glade_xml_get_widget(save_glade, "save_details_dialog");
        if ( widget ) {
            gtk_widget_hide(widget);
        }
        widget = glade_xml_get_widget(details_glade, "save_details_button");
        if ( widget ) {
            gtk_button_set_sensitive(widget, TRUE);
        }
        gtk_object_unref(GTK_OBJECT(save_glade));
        save_glade = NULL;
    }
}

void save_details_slot( GtkWidget* w, gpointer data )
{
    open_save_details();
}

void perform_save_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *widget;
    struct stat sb;
    FILE *fp;
    gchar *path;
    gchar *text;

    widget = glade_xml_get_widget(save_glade, "save_details_dialog");
    if ( widget ) {
        path = gtk_file_selection_get_filename(GTK_FILE_SELECTION(widget));
        if ( *path ) {
            /* Handle the setting of a directory */
            if ( stat(path, &sb) == 0 ) {
                if ( S_ISDIR(sb.st_mode) ) {
                    gtk_file_selection_set_filename(GTK_FILE_SELECTION(widget),
                                                    path);
                    return;
                }
            }
            widget = glade_xml_get_widget(details_glade, "update_details_text");
            fp = fopen(path, "w");
            if ( fp ) {
                text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
                fputs(text, fp);
                g_free(text);
                fclose(fp);
            } else {
                char message[PATH_MAX];
                snprintf(message, sizeof(message),
                         "Unable to write to %s\n", path);
                add_details_text(LOG_WARNING, message);
            }
        }
    }
    close_save_details();
}

void cancel_save_slot( GtkWidget* w, gpointer data )
{
    close_save_details();
}

void view_details_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *widget;
    
    widget = glade_xml_get_widget(details_glade, "details_dialog");
    if ( widget ) {
        gtk_widget_show(widget);
        widget = glade_xml_get_widget(update_glade, "list_details_button");
        if ( widget ) {
            gtk_button_set_sensitive(widget, FALSE);
        }
        widget = glade_xml_get_widget(update_glade, "update_details_button");
        if ( widget ) {
            gtk_button_set_sensitive(widget, FALSE);
        }
    }
}

void close_details_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *widget;
    
    widget = glade_xml_get_widget(details_glade, "details_dialog");
    if ( widget ) {
        gtk_widget_hide(widget);
    }
    widget = glade_xml_get_widget(update_glade, "list_details_button");
    if ( widget ) {
        gtk_button_set_sensitive(widget, TRUE);
    }
    widget = glade_xml_get_widget(update_glade, "update_details_button");
    if ( widget ) {
        gtk_button_set_sensitive(widget, TRUE);
    }
    close_save_details();
}

void view_gpg_details_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *widget;
    
    widget = glade_xml_get_widget(gpg_glade, "gpg_dialog");
    if ( widget ) {
        gtk_widget_show(widget);
        widget = glade_xml_get_widget(update_glade, "gpg_details_button");
        if ( widget ) {
            gtk_button_set_sensitive(widget, FALSE);
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
        gtk_button_set_sensitive(widget, TRUE);
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
        gtk_button_set_sensitive(widget, TRUE);
    }
}

static void fill_mirrors_list(urlset *mirrors)
{
    struct mirror_url *entry;
    GSList *radio_list = NULL;
    GtkWidget *widget;
    GtkWidget *hbox;
    GtkWidget *mirrors_vbox;
    char host[PATH_MAX];


    /* Get the area where we can put the list of mirrors */
    mirrors_vbox = glade_xml_get_widget(mirrors_glade, "mirrors_vbox");
    if ( ! mirrors_vbox ) {
        return;
    }

    /* Clear any previous list of mirrors */
    gtk_container_foreach(GTK_CONTAINER(mirrors_vbox),
                          gtk_empty_container, mirrors_vbox);

    /* Populate the vbox with the list of mirrors */
    for ( entry = mirrors->list; entry; entry = entry->next ) {
        if ( get_url_host(entry->url, host, sizeof(host)) ) {
            /* Create an hbox for this line */
            hbox = gtk_hbox_new(FALSE, 4);
            gtk_box_pack_start(GTK_BOX(mirrors_vbox), hbox, FALSE, FALSE, 0);
            gtk_widget_show(hbox);
            entry->data = hbox;

            /* Create a pixmap status icon */
            if ( entry->status == URL_OK ) {
                widget = gtk_pixmap_new(balls[1].pixmap, balls[1].bitmap);
            } else {
                widget = gtk_pixmap_new(balls[4].pixmap, balls[4].bitmap);
            }
            gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
            gtk_widget_show(widget);

            /* Create a radio button for this mirror */
            widget = gtk_radio_button_new_with_label(radio_list, host);
            gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
            radio_list = gtk_radio_button_group(GTK_RADIO_BUTTON(widget));
            if ( entry == mirrors->current ) {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
            }
            if ( entry->status == URL_OK ) {
                gtk_widget_set_sensitive(GTK_WIDGET(widget), TRUE);
            } else {
                gtk_widget_set_sensitive(GTK_WIDGET(widget), FALSE);
            }
            gtk_widget_show(widget);

            /* If this is a preferred mirror, note that in the UI */
            if ( mirrors->preferred_site &&
                 (strcasecmp(host, mirrors->preferred_site) == 0) ) {
                widget = gtk_label_new(_(" (preferred)"));
                gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
                gtk_misc_set_alignment(GTK_MISC(widget), 0, .5);
                gtk_widget_show(widget);
            }
        } else {
            entry->data = NULL;
        }
    }
}

static void select_current_mirror(urlset *mirrors)
{
    GtkWidget *hbox;
    GList *list;

    hbox = (GtkWidget *)mirrors->current->data;
    if ( hbox ) {
        list = gtk_container_children(GTK_CONTAINER(hbox));
        list = list->next;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(list->data), TRUE);
    }
}

static void failed_current_mirror(urlset *mirrors)
{
    GtkWidget *hbox;
    GList *list;

    hbox = (GtkWidget *)mirrors->current->data;
    if ( hbox ) {
        list = gtk_container_children(GTK_CONTAINER(hbox));
        gtk_pixmap_set(GTK_PIXMAP(list->data),
                       balls[4].pixmap, balls[4].bitmap);
        list = list->next;
        gtk_widget_set_sensitive(GTK_WIDGET(list->data), FALSE);
    }
    set_url_status(mirrors, URL_FAILED);
}

void show_mirrors_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *widget;

    widget = glade_xml_get_widget(mirrors_glade, "mirrors_dialog");
    if ( widget ) {
        gtk_widget_show(widget);
    }
}

void close_mirrors_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *widget;

    widget = glade_xml_get_widget(mirrors_glade, "mirrors_dialog");
    if ( widget ) {
        gtk_widget_hide(widget);
    }
}

void choose_mirror_slot( GtkWidget* w, gpointer data )
{
    urlset *mirrors;
    struct mirror_url *entry, *stop, *prev;
    GtkWidget *hbox;
    GList *list;

    /* If we didn't change the currently selected mirror, we're done */
    mirrors = update_patchset->mirrors; 
    entry = mirrors->current;
    hbox = (GtkWidget *)mirrors->current->data;
    if ( hbox ) {
        list = gtk_container_children(GTK_CONTAINER(hbox));
        list = list->next;
        if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(list->data)) ) {
            return;
        }
    }

    /* Find the toggled entry and mark it as the next mirror */
    stop = entry;
    do {
        prev = entry;
        entry = entry->next;
        if ( ! entry ) {
            entry = mirrors->list;
        }
        hbox = (GtkWidget *)entry->data;
        if ( hbox ) {
            list = gtk_container_children(GTK_CONTAINER(hbox));
            list = list->next;
            if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(list->data)) ) {
                switch_mirror = 1;
                mirrors->current = prev;
                break;
            }
        }
    } while ( entry != stop );
}

void switch_mirror_slot( GtkWidget* w, gpointer data )
{
    switch_mirror = 1;
}

void save_mirror_slot( GtkWidget* w, gpointer data )
{
    patchset *patchset;

    if ( update_patchset ) {
        for ( patchset=product_patchset; patchset; patchset=patchset->next ) {
            set_preferred_url(patchset->mirrors,
                              patchset->mirrors->current->url);
        }
        save_preferred_url(update_patchset->mirrors);
        fill_mirrors_list(update_patchset->mirrors);
    }
}

static void mirror_buttons_sensitive(gboolean sensitive)
{
    GtkWidget *widget;

    widget = glade_xml_get_widget(update_glade, "choose_mirror_button");
    if ( widget ) {
        gtk_widget_set_sensitive(widget, sensitive);
    }
    widget = glade_xml_get_widget(update_glade, "switch_mirror_button");
    if ( widget ) {
        gtk_widget_set_sensitive(widget, sensitive);
    }
    widget = glade_xml_get_widget(update_glade, "save_mirror_button");
    if ( widget ) {
        gtk_widget_set_sensitive(widget, sensitive);
    }
    if ( ! sensitive ) {
        close_mirrors_slot(NULL, NULL);
    }
}

void cancel_download_slot( GtkWidget* w, gpointer data )
{
    download_cancelled = 1;
}

void update_proceed_slot( GtkWidget* w, gpointer data )
{
    /* Stop any current flashing */
    stop_flash();

    if ( update_proceeding ) {
        main_menu_slot(NULL, NULL);
    } else {
        update_proceeding = 1;
    }
}

static void set_download_info(struct download_update_info *info,
                              GtkWidget *status,
                              GtkWidget *progress,
                              GtkWidget *rate_label,
                              GtkWidget *eta_label)
{
    info->status = status;
    info->progress = progress;
    info->rate_label = rate_label;
    info->eta_label = eta_label;
    info->last_rate = 0.0f;
    info->last_update = 0;
    switch_mirror = 0;
    download_cancelled = 0;
}

static void set_status_message(GtkWidget *status_label, const char *text)
{
    char *text_buf;

    if ( status_label ) {
        gtk_label_set_text(GTK_LABEL(status_label), text);
    }
    text_buf = (char *)safe_malloc(strlen(text)+2);
    sprintf(text_buf, "%s\n", text);
    add_details_text(LOG_STATUS, text_buf);
    free(text_buf);
}

static int download_update(int status_level, const char *status,
                           float percentage,
                           int size, int total, float rate, void *udata)
{
    struct download_update_info *info = (struct download_update_info *)udata;

    /* First show any status updates */
    if ( status ) {
        if ( status_level == LOG_STATUS ) {
            set_status_message(info->status, status);
        } else {
            add_details_text(status_level, status);
        }
    }

    /* Now update the progress bar */
    if ( info->progress && percentage ) {
        gtk_progress_set_percentage(GTK_PROGRESS(info->progress),
                                    percentage/100.0);
    }

    /* If we have actual download progress, show that too */
    if ( rate > 0.01f ) {
        float last_rate = info->last_rate;
        info->last_rate = rate;
        if ( rate < last_rate ) {
            if ( (rate - last_rate) > MAX_RATE_CHANGE ) {
                rate = 0.0f;
            }
        } else {
            if ( (last_rate - rate) > MAX_RATE_CHANGE ) {
                rate = 0.0f;
            }
        }
    }
    if ( (rate > 0.01f) && (time(NULL) != info->last_update) ) {
        char text[128];

        if ( info->rate_label ) {
            sprintf(text, "%7.2f K/s ", rate);
            gtk_label_set_text(GTK_LABEL(info->rate_label), text);
        }
        if ( total && info->eta_label ) {
            long eta = (long)((float)(total-size)/rate);

            if ( eta < (60*60*24) ) {
                if ( eta > (60*60) ) {
                    sprintf(text, _("ETA: %ld:"), eta/(60*60));
                    eta %= (60*60);
                } else {
                    sprintf(text, _("ETA: "));
                }
                sprintf(&text[strlen(text)], "%2.2ld:%2.2ld",
                        eta/60, eta%60);
            } else {
                strcpy(text, _("unknown"));
            }
            gtk_label_set_text(GTK_LABEL(info->eta_label), text);
        }
        info->last_update = time(NULL);
    }

    while( gtk_events_pending() ) {
        gtk_main_iteration();
    }
    return(download_cancelled || switch_mirror);
}

static gpg_result do_gpg_verify(const char *file, char *sig, int maxsig)
{
    gpg_result gpg_code;
    struct download_update_info info;
    GtkWidget *status;
    GtkWidget *verify;

    status = glade_xml_get_widget(update_glade, "update_status_label");
    set_status_message(status, _("Running GPG..."));
    while( gtk_events_pending() ) {
        gtk_main_iteration();
    }
    gpg_code = gpg_verify(file, sig, maxsig);
    if ( gpg_code == GPG_NOPUBKEY ) {
        verify = glade_xml_get_widget(update_glade, "verify_status_label");
        set_status_message(verify, _("Downloading public key"));
        set_download_info(&info, status, NULL, NULL, NULL);
        get_publickey(sig, download_update, &info);
        gpg_code = gpg_verify(file, sig, maxsig);
    }
    return gpg_code;
}

static void set_progress_url(GtkProgress *progress, const char *url)
{
    char text[1024], *bufp;
    int len;

    gtk_progress_set_show_text(GTK_PROGRESS(progress), TRUE);
    bufp = strstr(url, "://");
    if ( bufp ) {
        bufp += 3;
        while ( *bufp && (*bufp != '/') ) {
            ++bufp;
        }
        len = (bufp - url)+1;
        if ( len >= (sizeof(text)-3) ) {
            len = sizeof(text)-3-1;
        }
        strncpy(text, url, len);
        text[len] = '\0';
        strcat(text, "...");
        if ( *bufp ) {
            bufp = strrchr(url, '/');
            strncat(text, bufp, sizeof(text)-strlen(text));
        }
    } else {
        strncpy(text, url, sizeof(text));
    }
    gtk_progress_set_format_string(GTK_PROGRESS(progress), text);
}

static void calculate_update_size(void)
{
    GtkWidget *widget;
    int size;
    patchset *patchset;
    char text[1024];

    /* First find out how many MB we're talking about */
    size = 0;
    for ( patchset = product_patchset; patchset; patchset = patchset->next ) {
        size += (selected_size(patchset) + 1023) / 1024;
    }

    /* Now calculate the size label */
    sprintf(text, _("(%d MB)"), size);
    widget = glade_xml_get_widget(update_glade, "estimated_size_label");
    if ( widget ) {
        gtk_label_set_text(GTK_LABEL(widget), text);
    }
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
    calculate_update_size();
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

static void cleanup_update(const char *status_msg, int update_obsolete)
{
    GtkWidget *status;
    GtkWidget *action;
    GtkWidget *cancel;

    /* Remove the update patch file */
    close_readme_slot(NULL, NULL);
    if ( update_obsolete ) {
        remove_update();
        remove_readme();
    }
    update_proceeding = 0;

    /* Deselect the current patch path */
    select_node(update_patch->node, 0);

    /* We succeeded, enable the action button, and update the status */
    action = glade_xml_get_widget(update_glade, "update_action_button");
    cancel = glade_xml_get_widget(update_glade, "update_cancel_button");
    if ( (update_status >= 0) && skip_to_selected_update() ) {
        if ( cancel ) {
            gtk_button_set_sensitive(cancel, TRUE);
        }
        gtk_button_set_text(GTK_BUTTON(action), _("Continue"));
    } else {
        if ( cancel ) {
            gtk_button_set_sensitive(cancel, FALSE);
        }
        gtk_button_set_text(GTK_BUTTON(action), _("Finished"));
    }
    gtk_button_set_sensitive(action, TRUE);

    if ( status_msg ) {
        status = glade_xml_get_widget(update_glade, "update_status_label");
        set_status_message(status, status_msg);
    } else {
        /* User cancelled the update? */
        main_menu_slot(NULL, NULL);
        return;
    }

    /* Handle auto-update of the next set of patches, if any */
    if ( interactive != FULLY_INTERACTIVE ) {
        if ( update_status >= 0 ) {
            stop_flash();
            download_update_slot(NULL, NULL);
        }
    }
}

void action_button_slot(GtkWidget* w, gpointer data)
{
    /* Stop any current flashing */
    stop_flash();

    /* If there's a valid patch file, run it! */
    if ( update_proceeding ) {
        perform_update_slot(NULL, NULL);
    } else {
        /* We're done - do the next patch or quit */
        if ( update_status < 0 ) {
            main_menu_slot(NULL, NULL);
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
    /* Stop any current flashing */
    stop_flash();

    if ( download_pending ) {
        cancel_download_slot(NULL, NULL);
    } else {
        cleanup_update(NULL, 0);
    }
}

void download_update_slot( GtkWidget* w, gpointer data )
{
    struct download_update_info info;
    GtkWidget *notebook;
    GtkWidget *widget;
    GtkWidget *status;
    GtkWidget *verify;
    GtkWidget *action;
    GtkWidget *cancel;
    GtkWidget *progress;
    GtkWidget *gpg_status;
    patch *patch;
    char text[1024];
    const char *url;
    char sig[1024];
    char sum_file[PATH_MAX];
    char md5_real[CHECKSUM_SIZE+1];
    char md5_calc[CHECKSUM_SIZE+1];
    FILE *fp;
    gboolean have_readme;
    verify_result verified;

    /* Verify that we have an update to perform */
    patch = skip_to_selected_update();
    if ( ! patch ) {
        start_flash(1, 1);
        return;
    }
    patch->installed = 1;
    update_proceeding = 1;

    /* Set the current page to the product update page */
    notebook = glade_xml_get_widget(update_glade, "update_notebook");
    gtk_notebook_set_page(GTK_NOTEBOOK(notebook), UPDATE_PAGE);

    /* Set the action button to "Update" and disable it for now */
    status = glade_xml_get_widget(update_glade, "update_status_label");
    verify = glade_xml_get_widget(update_glade, "verify_status_label");
    action = glade_xml_get_widget(update_glade, "update_action_button");
    gtk_button_set_text(GTK_BUTTON(action), _("Update"));
    gtk_button_set_sensitive(action, FALSE);

    /* Show the initial status for this update */
    widget = glade_xml_get_widget(update_glade, "update_name_label");
    add_details_text(LOG_VERBOSE, "\n");
    snprintf(text, (sizeof text), "%s: %s",
             get_product_description(patch->patchset->product_name),
             patch->description);
    set_status_message(widget, text);

    /* Download the update from the server */
    update_arrows(1, 1);
    have_readme = FALSE;
    verified = DOWNLOAD_FAILED;
    download_pending = 1;
    randomize_urls(patch->patchset->mirrors);
    fill_mirrors_list(patch->patchset->mirrors);
    do {
        /* Grab the next URL to try */
        url = get_next_url(patch->patchset->mirrors, patch->file);
        if ( ! url ) {
            break;
        }
        select_current_mirror(patch->patchset->mirrors);

        /* Reset the panel */
        widget = glade_xml_get_widget(update_glade, "update_download_progress");
        if ( widget ) {
            gtk_progress_set_percentage(GTK_PROGRESS(widget), 0.0);
            gtk_progress_set_show_text(GTK_PROGRESS(widget), FALSE);
        }
        widget = glade_xml_get_widget(update_glade, "update_patch_progress");
        if ( widget ) {
            gtk_progress_set_percentage(GTK_PROGRESS(widget), 0.0);
        }
        widget = glade_xml_get_widget(update_glade, "update_rate_label");
        if ( widget ) {
            gtk_label_set_text(GTK_LABEL(widget), "");
        }
        widget = glade_xml_get_widget(update_glade, "update_eta_label");
        if ( widget ) {
            gtk_label_set_text(GTK_LABEL(widget), "");
        }
        if ( status ) {
            gtk_label_set_text(GTK_LABEL(status), "");
        }
        if ( verify ) {
            gtk_label_set_text(GTK_LABEL(verify), "");
        }
        gpg_status = glade_xml_get_widget(update_glade, "gpg_status_label");
        if ( gpg_status ) {
            gtk_label_set_text(GTK_LABEL(gpg_status), "");
        }
        cancel = glade_xml_get_widget(update_glade, "update_cancel_button");
        if ( cancel ) {
            gtk_button_set_sensitive(cancel, TRUE);
        }
        widget = glade_xml_get_widget(update_glade, "update_readme_button");
        if ( widget ) {
            gtk_button_set_sensitive(widget, have_readme);
        }
        widget = glade_xml_get_widget(update_glade, "gpg_details_button");
        if ( widget ) {
            gtk_button_set_sensitive(widget, FALSE);
        }
        update_balls(-1, 0);
    
        /* Set the mirror button state */
        if ( patch->patchset->mirrors->num_okay > 1 ) {
            mirror_buttons_sensitive(TRUE);
        } else {
            mirror_buttons_sensitive(FALSE);
        }

        /* Download the README and enable the button if we have a README */
        if ( ! have_readme && (interactive != FULLY_AUTOMATIC) ) {
            set_status_message(status, _("Downloading README"));
            widget = glade_xml_get_widget(update_glade,"update_download_label");
            if ( widget ) {
                gtk_label_set_text(GTK_LABEL(widget), _("Downloading README"));
            }
            sprintf(readme_file, "%s.txt", url);
            set_download_info(&info, status, NULL, NULL, NULL);
            if ( get_url(readme_file, readme_file, sizeof(readme_file),
                         download_update, &info) == 0 ) {
                widget = glade_xml_get_widget(update_glade, "update_readme_button");
                if ( widget ) {
                    gtk_button_set_sensitive(widget, TRUE);
                }
                have_readme = TRUE;
            } else {
                if ( switch_mirror ) {
                    continue;
                }
            }
        }
    
        /* Download the update */
        update_balls(1, 1);
        set_status_message(status, _("Downloading update"));
        widget = glade_xml_get_widget(update_glade,"update_download_label");
        if ( widget ) {
            gtk_label_set_text(GTK_LABEL(widget), _("Downloading update"));
        }
        strcpy(update_url, url);
        progress = glade_xml_get_widget(update_glade, "update_download_progress");
        set_progress_url(GTK_PROGRESS(progress), update_url);
        set_download_info(&info, status, progress,
            glade_xml_get_widget(update_glade, "update_rate_label"),
            glade_xml_get_widget(update_glade, "update_eta_label"));
        if ( get_url(update_url, update_url, sizeof(update_url),
                      download_update, &info) != 0 ) {
            /* Switch to the next available mirror */
            if ( switch_mirror ) {
                continue;
            }

            /* The download was cancelled or the download failed */
            failed_current_mirror(patch->patchset->mirrors);
            update_balls(1, 4);
            verified = DOWNLOAD_FAILED;
        } else {
            mirror_buttons_sensitive(FALSE);
            update_balls(1, 2);
            verified = VERIFY_UNKNOWN;
        }
    
        /* Verify the update */
        if ( verified == VERIFY_UNKNOWN ) {
            update_balls(2, 1);
        }
        /* First check the GPG signature */
        if ( verified == VERIFY_UNKNOWN ) {
            set_status_message(verify, _("Verifying GPG signature"));
            sprintf(sum_file, "%s.sig", url);
            set_download_info(&info, status, NULL, NULL, NULL);
            if ( get_url(sum_file, sum_file, sizeof(sum_file),
                         download_update, &info) == 0 ) {
                switch (do_gpg_verify(sum_file, sig, sizeof(sig))) {
                    case GPG_NOTINSTALLED:
                        set_status_message(gpg_status,
                                           _("GPG not installed"));
                        verified = VERIFY_UNKNOWN;
                        break;
                    case GPG_CANCELLED:
                        set_status_message(gpg_status,
                                           _("GPG was cancelled"));
                        verified = VERIFY_UNKNOWN;
                        break;
                    case GPG_NOPUBKEY:
                        set_status_message(gpg_status,
                                           _("GPG key not available"));
                        verified = VERIFY_UNKNOWN;
                        break;
                    case GPG_IMPORTED:
                        /* Used internally, never happens */
                        break;
                    case GPG_VERIFYFAIL:
                        failed_current_mirror(patch->patchset->mirrors);
                        set_status_message(gpg_status,
                                           _("GPG verify failed"));
                        verified = VERIFY_FAILED;
                        break;
                    case GPG_VERIFYOK:
                        set_status_message(gpg_status,
                                           _("GPG verify succeeded"));
                        enable_gpg_details(update_url, sig);
                        verified = VERIFY_OK;
                        break;
                }
            } else {
                set_status_message(gpg_status,
                                   _("GPG signature not available"));
            }
            unlink(sum_file);
        }
        /* Now download the MD5 checksum file */
        if ( verified == VERIFY_UNKNOWN ) {
            set_status_message(verify, _("Verifying MD5 checksum"));
            sprintf(sum_file, "%s.md5", url);
            set_download_info(&info, status, NULL, NULL, NULL);
            if ( get_url(sum_file, sum_file, sizeof(sum_file),
                         download_update, &info) == 0 ) {
                fp = fopen(sum_file, "r");
                if ( fp ) {
                    if ( fgets(md5_calc, sizeof(md5_calc), fp) ) {
                        set_status_message(status,
                                _("Calculating MD5 checksum"));
                        while( gtk_events_pending() ) {
                            gtk_main_iteration();
                        }
                        md5_compute(update_url, md5_real, 0);
                        if ( strcmp(md5_calc, md5_real) != 0 ) {
                            failed_current_mirror(patch->patchset->mirrors);
                            verified = VERIFY_FAILED;
                        }
                    }
                    fclose(fp);
                }
            } else {
                set_status_message(verify, _("MD5 checksum not available"));
            }
            unlink(sum_file);
        }
    } while ( ((verified == DOWNLOAD_FAILED) || (verified == VERIFY_FAILED)) &&
              !download_cancelled );
    download_pending = 0;
    mirror_buttons_sensitive(FALSE);

    /* We either ran out of update URLs or we downloaded a valid update */
    switch (verified) {
        case VERIFY_UNKNOWN:
            set_status_message(verify, _("Verification succeeded"));
            update_balls(2, 3);
            break;
        case VERIFY_OK:
            set_status_message(verify, _("Verification succeeded"));
            update_balls(2, 2);
            break;
        case VERIFY_FAILED:
            set_status_message(verify, _("Verification failed"));
            update_balls(2, 4);
            update_status = -1;
            start_flash(1, 3);
            cleanup_update(_("Update corrupted"), 1);
            return;
        case DOWNLOAD_FAILED:
            update_status = -1;
            start_flash(1, 3);
            cleanup_update(_("Unable to retrieve update"), 0);
            return;
    }
    start_flash(1, 1);
    set_status_message(status, _("Ready for update"));
    gtk_button_set_sensitive(action, TRUE);

    /* Wait for the user to confirm the update (unless auto-updating) */
    if ( interactive != FULLY_INTERACTIVE ) {
        stop_flash();
        perform_update_slot(NULL, NULL);
    }
}

void perform_update_slot( GtkWidget* w, gpointer data )
{
    struct download_update_info info;
    GtkWidget *action;
    GtkWidget *status;
    GtkWidget *cancel;
    GtkWidget *progress;

    /* Actually perform the update */
    update_arrows(1, 1);
    update_balls(3, 1);
    action = glade_xml_get_widget(update_glade, "update_action_button");
    if ( action ) {
        gtk_button_set_sensitive(action, FALSE);
    }
    status = glade_xml_get_widget(update_glade, "update_status_label");
    set_status_message(status, _("Performing update"));
    cancel = glade_xml_get_widget(update_glade, "update_cancel_button");
    if ( cancel ) {
        gtk_button_set_sensitive(cancel, FALSE);
    }
    progress = glade_xml_get_widget(update_glade, "update_patch_progress");
    set_download_info(&info, status, progress, NULL, NULL);
    if ( perform_update(update_url,
                        get_product_root(update_patchset->product_name),
                        download_update, &info) != 0 ) {
        update_balls(3, 4);
        update_status = -1;
        start_flash(1, 3);
        cleanup_update(_("Update failed"), 0);
        return;
    }
    update_balls(3, 2);

    /* We're done!  A successful update! */
    start_flash(1, 1);
    ++update_status;
    cleanup_update(_("Update complete"), 1);
}

void select_all_updates_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *widget;
    GList *list, *poopy, *update_list;
    GtkWidget *button;

    widget = glade_xml_get_widget(update_glade, "update_vbox");
    list = gtk_container_children(GTK_CONTAINER(widget));
    while ( list ) {
        widget = GTK_WIDGET(list->data);
        poopy = gtk_container_children(GTK_CONTAINER(widget));
        widget = GTK_WIDGET(poopy->data);
        update_list = gtk_container_children(GTK_CONTAINER(widget));
        while ( update_list ) {
            button = GTK_WIDGET(update_list->data);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
            update_list = update_list->next;
        }
        list = list->next;
    }
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
            gtk_button_set_sensitive(widget, TRUE);
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
            gtk_button_set_sensitive(widget, selected);
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

void toggle_auto_update_slot( GtkWidget* w, gpointer data )
{
    if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)) ) {
        interactive = SEMI_AUTOMATIC;
    } else {
        interactive = FULLY_INTERACTIVE;
    }
    save_interactive(interactive);
}

void choose_update_slot( GtkWidget* w, gpointer data )
{
    struct download_update_info info;
    GtkWidget *window;
    GtkWidget *notebook;
    GtkWidget *widget;
    GtkWidget *status;
    GtkWidget *update_vbox;
    GtkWidget *frame;
    GtkWidget *vbox;
    GtkWidget *button;
    GtkWidget *progress;
    patchset *patchset;
    const char *product_name;
    char text[1024];
    version_node *node, *root, *trunk;
    int selected;

    /* Set the current page to the patch choosing page */
    notebook = glade_xml_get_widget(update_glade, "update_notebook");
    gtk_notebook_set_page(GTK_NOTEBOOK(notebook), GETLIST_PAGE);

    /* Make sure the window is visible */
    window = glade_xml_get_widget(update_glade, TOPLEVEL);
    gtk_widget_realize(window);
    load_images(window);

    /* Get the update option list widget */
    update_vbox = glade_xml_get_widget(update_glade, "update_vbox");
    
    /* Clear any previous list of updates */
    gtk_container_foreach(GTK_CONTAINER(update_vbox), gtk_empty_container, update_vbox);

    /* Show the initial status for this operation */
    status = glade_xml_get_widget(update_glade, "list_status_label");
    clear_details_text();
    add_details_text(LOG_VERBOSE, "\n");
    set_status_message(status, _("Listing product updates"));

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
        add_details_text(LOG_VERBOSE, "\n");
        widget = glade_xml_get_widget(update_glade, "product_label");
        set_status_message(widget, get_product_description(product_name));
        widget = glade_xml_get_widget(update_glade, "update_list_progress");
        if ( widget ) {
            gtk_progress_set_percentage(GTK_PROGRESS(widget), 0.0);
        }
        widget = glade_xml_get_widget(update_glade, "list_rate_label");
        if ( widget ) {
            gtk_label_set_text(GTK_LABEL(widget), "");
        }
        widget = glade_xml_get_widget(update_glade, "list_eta_label");
        if ( widget ) {
            gtk_label_set_text(GTK_LABEL(widget), "");
        }
        if ( status ) {
            gtk_label_set_text(GTK_LABEL(status), "");
        }
        widget = glade_xml_get_widget(update_glade, "list_cancel_button");
        if ( widget ) {
            gtk_button_set_sensitive(widget, TRUE);
        }
        widget = glade_xml_get_widget(update_glade, "list_done_button");
        if ( widget ) {
            gtk_button_set_sensitive(widget, FALSE);
            gtk_button_set_text(GTK_BUTTON(widget), _("Continue"));
        }
    
        /* Download the patch list */
        update_arrows(0, 1);
        update_balls(0, 1);
        strcpy(update_url, get_product_url(patchset->product_name));
        progress = glade_xml_get_widget(update_glade, "update_list_progress");
        set_progress_url(GTK_PROGRESS(progress), update_url);
        set_download_info(&info, status, progress,
            glade_xml_get_widget(update_glade, "list_rate_label"),
            glade_xml_get_widget(update_glade, "list_eta_label"));
        if ( get_url(update_url, update_url, sizeof(update_url),
                      download_update, &info) != 0 ) {
            remove_update();
            update_balls(0, 4);
            /* Tell the user what happened, and wait before continuing */
            if ( download_cancelled ) {
                set_status_message(status, _("Download cancelled"));
            } else {
                set_status_message(status, _("Unable to retrieve update list"));
            }
            if ( interactive != FULLY_AUTOMATIC ) {
                widget=glade_xml_get_widget(update_glade, "list_cancel_button");
                if ( widget ) {
                    gtk_button_set_sensitive(widget, FALSE);
                }
                widget = glade_xml_get_widget(update_glade, "list_done_button");
                if ( widget ) {
                    gtk_button_set_sensitive(widget, TRUE);
                }
                update_proceeding = 0;
                start_flash(0, 3);
                do {
                    gtk_main_iteration();
                } while ( ! update_proceeding );
            }
            free_patchset(patchset);
            continue;
        }
        set_status_message(status, _("Retrieved update list"));
        update_arrows(0, 1);
        update_balls(0, 2);
        widget=glade_xml_get_widget(update_glade, "list_cancel_button");
        if ( widget ) {
            gtk_button_set_sensitive(widget, FALSE);
        }
        while( gtk_events_pending() ) {
            gtk_main_iteration();
        }
    
        /* Turn the patch list into a set of patches */
        load_patchset(patchset, update_url);
        remove_update();
    
        /* If there are no patches, we're done with this product */
        if ( ! patchset->patches ) {
            free_patchset(patchset);
            continue;
        }
    
        /* Add a frame and label for this product */
        snprintf(text, sizeof(text), "%s %s",
                 get_product_description(product_name),
                 get_product_version(product_name));
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
                    strncpy(text, node->description, sizeof(text));
                    if ( node->note ) {
                        int textlen;
                        textlen = strlen(text);
                        snprintf(&text[textlen], sizeof(text)-textlen,
                                 " (%s)", node->note);
                    }
                    button = gtk_check_button_new_with_label(text);
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

    /* See if there are any updates available */
    if ( ! product_patchset ||
         ((interactive == FULLY_AUTOMATIC) && ! update_patch) ) {
        /* The continue button becomes a finished button, no updates */
        set_status_message(status, _("No new updates available"));
        update_proceeding = 1;
        if ( interactive != FULLY_AUTOMATIC ) {
            start_flash(0, 1);
        }
        widget = glade_xml_get_widget(update_glade, "list_done_button");
        if ( widget ) {
            gtk_button_set_text(GTK_BUTTON(widget), _("Finished"));
        }
        gtk_button_set_sensitive(widget, TRUE);
    } else {
        /* Switch the notebook to the appropriate page */
        gtk_notebook_set_page(GTK_NOTEBOOK(notebook), SELECT_PAGE);

        /* Disable the continue button until an option is selected */
        widget = glade_xml_get_widget(update_glade, "choose_continue_button");
        if ( widget ) {
            gtk_button_set_text(GTK_BUTTON(widget), _("Continue"));
            if ( selected ) {
                gtk_button_set_sensitive(widget, TRUE);
            } else {
                gtk_button_set_sensitive(widget, FALSE);
            }
        }

        /* Set the state of the Quick-Update checkbox */
        if ( interactive != FULLY_AUTOMATIC ) {
            widget = glade_xml_get_widget(update_glade, "auto_update_toggle");
            if ( widget ) {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
                                             (interactive == SEMI_AUTOMATIC));
            }
        }

        /* Handle auto-update mode */
        if ( interactive == FULLY_AUTOMATIC ) {
            download_update_slot(NULL, NULL);
        }
    }
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
        gtk_button_set_sensitive(widget, enabled);
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
    GtkWidget *label;
    const char *product_name;
    const char *description;

    gtk_init(&argc,&argv);

    /* Initialize Glade */
    glade_init();
    update_glade = glade_xml_new(UPDATE_GLADE, TOPLEVEL); 
    mirrors_glade = glade_xml_new(UPDATE_GLADE, "mirrors_dialog");
    gpg_glade = glade_xml_new(UPDATE_GLADE, "gpg_dialog");
    details_glade = glade_xml_new(UPDATE_GLADE, "details_dialog");

    /* Add all signal handlers defined in glade file */
    glade_xml_signal_autoconnect(update_glade);
    glade_xml_signal_autoconnect(mirrors_glade);
    glade_xml_signal_autoconnect(gpg_glade);
    glade_xml_signal_autoconnect(details_glade);

    /* Fill in the list of products */
    widget = glade_xml_get_widget(update_glade, "product_vbox");
    if ( widget ) {
        for ( product_name=get_first_product();
              product_name;
              product_name=get_next_product() ) {
            description = get_product_description(product_name);
            button = gtk_check_button_new_with_label(description);
            gtk_object_set_data(GTK_OBJECT(button), "data",
                                (gpointer)product_name);
            gtk_box_pack_start(GTK_BOX(widget), button, FALSE, FALSE, 0);
            if ( strcasecmp(PRODUCT, product_name) != 0 ) {
                gtk_signal_connect(GTK_OBJECT(button), "toggled",
                    GTK_SIGNAL_FUNC(product_toggle_option), (gpointer)0);
                gtk_widget_show(button);
            }
        }
        if ( get_num_products() == 0 ) {
            label = gtk_label_new(
_("No products found.\nAre you the one that installed the software?"));
            gtk_box_pack_start(GTK_BOX(widget), label, FALSE, TRUE, 0);
            gtk_widget_show(label);
        }
    } else {
        log(LOG_ERROR, _("No product_vbox in glade file!\n"));
        return(-1);
    }
    widget = glade_xml_get_widget(update_glade, "product_continue_button");
    if ( widget ) {
        gtk_button_set_sensitive(widget, FALSE);
    }

    /* Tell the patches we're applying that they should be verbose, without
       resorting to command-line hackery.
    */
    putenv("PATCH_LOGGING=1");

    return 0;
}

static int gtkui_update_product(const char *product)
{
    update_status = 0;
    select_product(product);
    one_product = 1;
#if 0 /* Don't go fully automatic when performing a UI update */
    interactive = FULLY_AUTOMATIC;
#else
    interactive = SEMI_AUTOMATIC;
#endif
    choose_update_slot(NULL, NULL);
    return update_status;
}

static int gtkui_perform_updates(const char *product)
{
    /* All errors go through our details window, don't print extra output */
    if ( get_logging() >= LOG_NORMAL ) {
        set_logging(LOG_NONE);
    }

    update_status = 0;
    interactive = load_interactive();
    if ( product ) {
        if ( is_valid_product(product) ) {
            select_product(product);
            one_product = 1;
            choose_update_slot(NULL, NULL);
        } else {
            GtkWidget *widget;
            GtkWidget *label;
            char message[PATH_MAX];

            widget = glade_xml_get_widget(update_glade, "product_vbox");
            snprintf(message, sizeof(message),
                     _("\"%s\" not found, are you the one who installed it?"),
                     product);
            label = gtk_label_new(message);
            gtk_box_pack_start(GTK_BOX(widget), label, FALSE, TRUE, 0);
            gtk_widget_show(label);
            choose_product_slot(NULL, NULL);
        }
    } else {
        one_product = 0;
        choose_product_slot(NULL, NULL);
    }
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

    /* Free images */
    free_images();

    /* Clean up the Glade files */
    gtk_object_unref(GTK_OBJECT(update_glade));
    gtk_object_unref(GTK_OBJECT(mirrors_glade));
    gtk_object_unref(GTK_OBJECT(gpg_glade));
    gtk_object_unref(GTK_OBJECT(details_glade));
}

update_UI gtk_ui = {
    gtkui_detect,
    gtkui_init,
    gtkui_update_product,
    gtkui_perform_updates,
    gtkui_cleanup
};
