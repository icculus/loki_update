
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "patchset.h"
#include "setupdb.h"
#include "load_patchset.h"
#include "update.h"
#include "log_output.h"

#define PRODUCT             "loki_update"

struct product_entry {
    patchset *patchset;
    struct product_entry *next;
} *product_list = NULL;

static void print_usage(char *argv0)
{
    fprintf(stderr, "Usage: %s [product]\n", argv0);
}

static int add_product_entry(const char *product)
{
    int status;

    status = 0;
    if ( strcmp(product, PRODUCT) != 0 ) {
        struct product_entry *entry;

        entry = (struct product_entry *)malloc(sizeof *entry);
        if ( entry ) {
            entry->patchset = load_patchset(product);
            if ( entry->patchset ) {
                entry->next = product_list;
                product_list = entry;
            } else {
                free(entry);
                status = -1;
            }
        } else {
            log(LOG_ERROR, "Out of memory, skipping product: %s\n", product);
            status = -1;
        }
    }
    return status;
}

static void generate_product_list(void)
{
    const char *product;
    struct product_entry *entry;

    /* First free the existing list, if any */
    while ( product_list ) {
        entry = product_list;
        product_list = product_list->next;

        free_patchset(entry->patchset);
        free(entry);
    }

    /* Generate a new list of products */
    for ( product=loki_getfirstproduct();
          product;
          product=loki_getnextproduct() ) {    
        add_product_entry(product);
    }
}

static patch *show_product_list(void)
{
    /* Show the list of products and patch paths */
    // TODO

    /* Choose a product and path to update */
    // TODO

    return(NULL);
}

int main(int argc, char *argv[])
{
    patchset *patchset;
    patch *patch;

    /* Initialize the UI */
    // TODO

    /* Stage 1: Update ourselves, if necessary */
    {
        patchset = load_patchset(PRODUCT);
        if ( ! patchset ) {
            return(255);
        }
        switch (auto_update(patchset)) {
            /* An error? return an error code */
            case -1:
                return(255);
            /* No update needed?  Continue.. */
            case 0:
                break;
            /* Patched ourselves, restart */
            default:
                execvp(argv[0], argv);
                fprintf(stderr, "Couldn't exec ourselves!  Exiting\n");
                return(255);
        }
        free_patchset(patchset);
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
            print_usage(argv[0]);
            return(1);
        }
        patchset = load_patchset(argv[i+1]);
        if ( ! patchset ) {
            return(2);
        }
        status = 0;
        switch (auto_update(patchset)) {
            /* An error? return an error code */
            case -1:
                status = 3;
                break;
            /* Succeeded, or no patch needed */
            default:
                break;
        }
        free_patchset(patchset);
        return(status);
    }

    /* Stage 3: Generate a list of products and update them */
    do {
        generate_product_list();
        patch = show_product_list();
        if ( perform_update(patch) < 0 ) {
            return(3);
        }
    } while ( patch );

    return(0);
}
