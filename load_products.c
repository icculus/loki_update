
#include "safe_malloc.h"
#include "load_products.h"


static struct product_list {
    char *product;
    char *version;
    char *description;
    struct product_list *next;
} *product_list = NULL;

static struct product_list *current = NULL;

static int is_listed(const char *product)
{
    struct product_list *entry;
    int listed;

    listed = 0;
    for ( entry = product_list; entry && !listed; entry = entry->next ) {
        if ( strcasecmp(entry->product, product) == 0 ) {
            listed = 1;
        }
    }
    return(listed);
}

static void add_product(const char *product)
{
    struct product_list *entry;

    /* Nothing to do if this is already listed */
    if ( is_listed(product) ) {
        return;
    }

    entry = (struct product_list *)safe_malloc(sizeof *entry);
        
}

void load_product_list(void)
{
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

void free_product_list(void)
{
}
