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

#include <stdio.h>
#include <string.h>

#include "setupdb.h"
#include "safe_malloc.h"
#include "log_output.h"
#include "update_ui.h"
#include "load_products.h"


typedef struct product_entry {
    char *product;
    char *version;
    char *description;
    char *root;
    char *update_url;
    char *default_component;
    struct product_entry *next;
} product_entry;

static int num_products;
static const char *override_update_url;
static product_entry *product_list = NULL;
static product_entry *current = NULL;

static product_entry *find_product(const char *product)
{
    product_entry *entry;

    for ( entry = product_list; entry; entry = entry->next ) {
        if ( strcasecmp(entry->product, product) == 0 ) {
            break;
        }
    }
    return(entry);
}

static void add_product(const char *product, const char *version,
                        const char *description, const char *root,
                        const char *update_url, const char *default_component)
{
    product_entry *new_entry, *entry, *prev;

    /* Create the entry */
    log(LOG_DEBUG, _("Adding product entry for '%s'\n"), product);
    new_entry = (product_entry *)safe_malloc(sizeof *entry);
    new_entry->product = safe_strdup(product);
    new_entry->version = safe_strdup(version);
    new_entry->description = safe_strdup(description);
    new_entry->root = safe_strdup(root);
    new_entry->update_url = safe_strdup(update_url);
    new_entry->default_component = safe_strdup(default_component);

    /* Insert it into the list, sorted alphabetically */
    prev = NULL;
    for ( entry = product_list; entry; entry = entry->next ) {
        if ( strcasecmp(entry->product, product) > 0 ) {
            new_entry->next = entry;
            if ( prev ) {
                prev->next = new_entry;
            } else {
                product_list = new_entry;
            }
            break;
        }
        prev = entry;
    }
    if ( ! entry ) {
        new_entry->next = NULL;
        if ( prev ) {
            prev->next = new_entry;
        } else {
            product_list = new_entry;
        }
    }
    ++num_products;
}

static char *get_line(char *line, int maxlen, FILE *file)
{
    line = fgets(line, maxlen, file);
    if ( line ) {
        line[strlen(line)-1] = '\0';
    }
    return(line);
}

static void detect_product(const char *product_name)
{
    FILE *detect;
    char command[1024];
    char version[1024];
    char description[1024];
    char root[1024];
    char update_url[1024];

    sprintf(command, "sh ./detect/detect.sh %s", product_name);
    detect = popen(command, "r");
    if ( detect ) {
        if ( get_line(version, sizeof(version), detect) &&
             get_line(description, sizeof(description), detect) &&
             get_line(root, sizeof(root), detect) &&
             get_line(update_url, sizeof(update_url), detect) ) {
            add_product(product_name, version,
                        description, root, update_url, "Default Install");
        } else {
            log(LOG_DEBUG, _("Failed scan for product '%s'\n"), product_name);
        }
        pclose(detect);
    }
}

static void load_detected_products(const char *wanted)
{
    FILE *list;
    char product_name[1024];

    /* If we want something in particular, look for that */
    if ( wanted ) {
        detect_product(wanted);
        return;
    }

    /* Otherwise scan for all known legacy products */
    list = fopen("detect/products.txt", "r");
    if ( ! list ) {
        /* No worries, I guess there's nothing to detect */
        return;
    }
    while ( get_line(product_name, sizeof(product_name), list) ) {

        /* If blank line, or we already have it, don't scan */
        if ( ! *product_name || find_product(product_name) ) {
            continue;
        }

        /* Open up the detection routine */
        detect_product(product_name);
    }
}

void load_product_list(const char *wanted)
{
    int found;
    const char *product_name;
    product_t *product;
    product_component_t *component;
    product_info_t *info;

    printf(_("Searching for installed products... ")); fflush(stdout);

    /* First load the "official" installed product list */
    num_products = 0;
    found = 0;
    for ( product_name = loki_getfirstproduct();
          product_name;
          product_name = loki_getnextproduct() ) {
        /* Skip unwanted entries, if we're looking for something */
        if ( wanted &&
             (strcasecmp(wanted, product_name) != 0) &&
             (strcasecmp(PRODUCT, product_name) != 0) ) {
            continue;
        }
        product = loki_openproduct(product_name);
        if ( product ) {
            info = loki_getinfo_product(product);
            component = loki_getdefault_component(product);
            if ( component && 
                 (strcmp(loki_getversion_component(component), "0") != 0) ) {
                add_product(info->name,
                            loki_getversion_component(component),
                            info->description, info->root, info->url,
                            loki_getname_component(component));
                if ( wanted && (strcasecmp(wanted, product_name) == 0) ) {
                    found = 1;
                }
            }
            loki_closeproduct(product);
        }
    }

    /* Now see what non-official products we should scan for */
    if ( ! found ) {
        load_detected_products(wanted);
    }

    printf(_("done!\n"));
}

int get_num_products(void)
{
    return(num_products);
}

const char *get_first_product(void)
{
    const char *product;

    current = product_list;
    if ( current ) {
        product = current->product;
    } else {
        product = NULL;
    }
    return(product);
}

const char *get_next_product(void)
{
    const char *product;

    current = current->next;
    if ( current ) {
        product = current->product;
    } else {
        product = NULL;
    }
    return(product);
}

int is_valid_product(const char *product)
{
    int valid;

    if ( find_product(product) ) {
        valid = 1;
    } else {
        valid = 0;
    }
    return(valid);
}

void set_override_url(const char *update_url)
{
    override_update_url = update_url;
}

void set_product_root(const char *product, const char *root)
{
    product_entry *entry;

    entry = find_product(product);
    if ( entry ) {
        free(entry->root);
        entry->root = safe_strdup(root);
    }
}

void set_product_url(const char *product, const char *url)
{
    product_entry *entry;

    entry = find_product(product);
    if ( entry ) {
        free(entry->update_url);
        entry->update_url = safe_strdup(url);
    }
}

const char *get_product_version(const char *product)
{
    product_entry *entry;
    const char *version;

    entry = find_product(product);
    if ( entry ) {
        version = entry->version;
    } else {
        version = NULL;
    }
    return(version);
}

const char *get_product_description(const char *product)
{
    product_entry *entry;
    const char *description;

    entry = find_product(product);
    if ( entry ) {
        description = entry->description;
    } else {
        description = NULL;
    }
    return(description);
}

const char *get_product_root(const char *product)
{
    product_entry *entry;
    const char *root;

    entry = find_product(product);
    if ( entry ) {
        root = entry->root;
    } else {
        root = NULL;
    }
    return(root);
}

const char *get_product_url(const char *product)
{
    product_entry *entry;
    const char *url;

    entry = find_product(product);
    if ( entry ) {
        if ( override_update_url ) {
            url = override_update_url;
        } else {
            url = entry->update_url;
        }
    } else {
        url = NULL;
    }
    return(url);
}

const char *get_default_component(const char *product)
{
    product_entry *entry;
    const char *component;

    entry = find_product(product);
    if ( entry ) {
        component = entry->default_component;
    } else {
        component = NULL;
    }
    return(component);
}

void free_product_list(void)
{
    product_entry *entry;

    while ( product_list ) {
        entry = product_list;
        product_list = product_list->next;
        free(entry->product);
        free(entry->version);
        free(entry->description);
        free(entry->root);
        free(entry->update_url);
        free(entry->default_component);
        free(entry);
    }
}
