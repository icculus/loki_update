
#include <stdlib.h>
#include <time.h>

#include "safe_malloc.h"
#include "urlset.h"


typedef struct update_url {
    char *url;
    struct update_url *next;
} url_list;

struct url_bucket {
    int order;
    int num_left;
    url_list *untried;
    url_list *tried;
    struct url_bucket *next;
};


static url_bucket_list *create_bucket(int order)
{
    url_bucket_list *list;

    list = (url_bucket_list *)safe_malloc(sizeof *list);
    list->order = order;
    list->num_left = 0;
    list->untried = NULL;
    list->tried = NULL;
    list->next = NULL;
    return(list);
}

static void add_to_bucket(url_bucket_list *list, const char *url)
{
    url_list *entry;

    entry = (url_list *)safe_malloc(sizeof *entry);
    entry->url = safe_strdup(url);
    entry->next = list->untried;
    list->untried = entry;
    ++list->num_left;
}

static void free_bucket(url_bucket_list *list)
{
    url_list *entry;

    if ( list ) {
        free_bucket(list->next);
        while ( list->tried ) {
            entry = list->tried;
            list->tried = list->tried->next;
            free(entry->url);
            free(entry);
        }
        while ( list->untried ) {
            entry = list->untried;
            list->untried = list->untried->next;
            free(entry->url);
            free(entry);
        }
        free(list);
    }
}

/* Create a set of update URLs */
urlset *create_urlset(void)
{
    urlset *urlset;

    /* Seed the random number generator for choosing URLs */
    srand(time(NULL));

    /* Create and return an empty set of URLs */
    urlset = (struct urlset *)safe_malloc(sizeof *urlset);
    urlset->urls = NULL;
    urlset->current = NULL;
    return(urlset);
}

/* Add a URL to a set of update URLs */
void add_url(urlset *urlset, const char *url, int order)
{
    url_bucket_list *list, *prev;

    prev = NULL;
    for ( list=urlset->urls; list && (list->order < order); list=list->next ) {
        prev = list;
    }
    if ( list ) {
        if ( list->order == order ) {
            /* Great, add to the current list */
            add_to_bucket(list, url);
        } else {
            /* Create a new list for this order */
            list = create_bucket(order);
            add_to_bucket(list, url);
        }
    } else {
        /* End of the line, create a tail list */
        list = create_bucket(order);
        add_to_bucket(list, url);
        if ( prev ) {
            list->next = prev->next;
            prev->next = list;
        } else {
            list->next = urlset->urls;
            urlset->urls = list;
        }
    }
    if ( ! urlset->current ) {
        urlset->current = urlset->urls;
    }
}

/* Get the next URL to be tried for an update */
const char *get_next_url(urlset *urlset)
{
    int index;
    url_bucket_list *urls;
    url_list *entry, *prev;
    const char *url;

    urls = urlset->current;
    if ( urls ) {
        /* Choose a random URL out of the ones left */
        prev = NULL;
        entry = urls->untried;
        for ( index = rand()%urls->num_left; index > 0; --index ) {
            prev = entry;
            entry = entry->next;
        }

        /* Move it from the untried list to the tried list */
        if ( prev ) {
            prev->next = entry->next;
        } else {
            urls->untried = entry->next;
        }
        entry->next = urls->tried;
        urls->tried = entry;

        /* Go to the next bucket of urls, if this one is empty */
        if ( ! --urls->num_left ) {
            urlset->current = urls->next;
        }
        url = entry->url;
    } else {
        url = NULL;
    }
    return(url);
}

/* Free a set of update URLs */
void free_urlset(urlset *urlset)
{
    if ( urlset ) {
        free_bucket(urlset->urls);
        free(urlset);
    }
}
