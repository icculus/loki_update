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
#include <limits.h>

#include "safe_malloc.h"
#include "prefpath.h"
#include "log_output.h"
#include "urlset.h"


/* Create a set of mirror URLs */
urlset *create_urlset(void)
{
    urlset *mirrors;
    FILE *fp;
    char preferred_mirror[PATH_MAX];

    /* Allocate the set of URLs */
    mirrors = (urlset *)safe_malloc(sizeof *mirrors);
    mirrors->num_mirrors = 0;
    mirrors->list = NULL;
    mirrors->current = NULL;
    mirrors->full_url[0] = '\0';

    /* Read the preferred mirror site */
    mirrors->preferred_site = NULL;
    preferences_path("preferred_mirror.txt",
                     preferred_mirror, sizeof(preferred_mirror));
    fp = fopen(preferred_mirror, "r");
    if ( fp ) {
        if ( fgets(preferred_mirror, sizeof(preferred_mirror), fp) ) {
            preferred_mirror[strlen(preferred_mirror)-1] = '\0';
            mirrors->preferred_site = safe_strdup(preferred_mirror);
        }
        fclose(fp);
    }
    return(mirrors);
}

/* Add a URL to a set of update URLs */
void add_url(urlset *urlset, const char *url)
{
    struct mirror_url *mirror;

    /* Allocate the URL structure */
    mirror = (struct mirror_url *)safe_malloc(sizeof *mirror);
    mirror->url = safe_strdup(url);
    mirror->status = URL_OK;
    mirror->next = NULL;

    /* Add the URL to our list */
    ++urlset->num_mirrors;
    if ( urlset->current ) {
        urlset->current->next = mirror;
    } else {
        urlset->list = mirror;
    }
    urlset->current = mirror;
}

/* Get the host from a URL, returning 1 or 0 if there is no host */
int get_url_host(const char *url, char *dst, int maxlen)
{
    int i;
    const char *host;

    i = 0;
    if ( *url != '/' ) {
        host = strstr(url, ":/");
        if ( host ) {
            host += 2;
            while ( *host == '/' ) {
                ++host;
            }
            while ( *host && (*host != '/') && (i < (maxlen-1)) ) {
                dst[i++] = *host++;
            }
        }
    }
    dst[i] = '\0';
    return(i != 0);
}

/* Randomize the order of the mirrors */
void randomize_urls(urlset *urlset)
{
    struct mirror_url **list, *current;
    int i, index, left;
    char host[PATH_MAX];

    /* If there is less than two mirrors, there's nothing to do */
    if ( urlset->num_mirrors < 2 ) {
        return;
    }

    /* Create a list of pointers we can sort */
    list = (struct mirror_url **)safe_malloc(urlset->num_mirrors*(sizeof *list));
    memset(list, 0, urlset->num_mirrors*(sizeof *list));

    /* Now we go through and move the preferred URLs to the new list */
    index = 0;
    if ( urlset->preferred_site ) {
        for ( current = urlset->list; current; current = current->next ) {
            if ( get_url_host(current->url, host, sizeof(host)) &&
                 (strcasecmp(host, urlset->preferred_site) == 0) ) {
                list[index++] = current;
                current->status = URL_USED;
            }
        }
    }
    left = urlset->num_mirrors - index;

    /* Randomize the rest of the URLs */
    for ( current = urlset->list; current; current = current->next ) {
        if ( current->status == URL_USED ) {
            continue;
        }
        /* Figure out the new spot in the list */
        index = -1;
        i = (rand()%left)+1;
        do {
            ++index;
            if ( ! list[index] ) {
                --i;
            }
        } while ( i > 0 );

        /* Add the current url to the list */
        --left;
        list[index] = current;
        current->status = URL_USED;
    }

    /* Now turn our list into a real URL list */
    for ( i=0; i<(urlset->num_mirrors-1); ++i ) {
        list[i]->next = list[i+1];
    }
    list[i]->next = NULL;
    urlset->list = list[0];
    reset_urlset(urlset);
    free(list);
}

static const char *get_current_url(urlset *urlset, const char *file)
{
    const char *url;

    /* Skip past URLs marked bad */
    if ( urlset->current->status != URL_OK ) {
        struct mirror_url *stop;

        stop = urlset->current;
        do {
            urlset->current = urlset->current->next;
            if ( ! urlset->current ) {
                urlset->current = urlset->list;
            }
        } while ( (urlset->current != stop) &&
                  (urlset->current->status != URL_OK) );
    }

    /* If we found a valid mirror, use it */
    if ( urlset->current->status == URL_OK ) {
        sprintf(urlset->full_url, "%s/%s", urlset->current->url, file);
        url = urlset->full_url;
    } else {
        urlset->current = NULL;
        url = NULL;
    }
    return(url);
}

/* Get the next URL to be tried for an update */
const char *get_next_url(urlset *urlset, const char *file)
{
    /* Sanity check */
    if ( urlset->num_mirrors == 0 ) {
        return(NULL);
    }
    if ( urlset->current ) {
        urlset->current = urlset->current->next;
        if ( ! urlset->current ) {
            urlset->current = urlset->list;
        }
    } else {
        urlset->current = urlset->list;
    }
    return(get_current_url(urlset, file));
}

/* Set the status of the current URL */
void set_url_status(urlset *urlset, enum url_status status)
{
    /* Sanity check */
    if ( ! urlset->current ) {
        return;
    }
    urlset->current->status = status;
}

/* Use the current URL as the preferred URL */
void current_url_preferred(urlset *urlset)
{
    FILE *fp;
    char mirror[PATH_MAX];

    /* Sanity check */
    if ( ! urlset->current ) {
        return;
    }

    /* Save the new preferred mirror site */
    if ( get_url_host(urlset->full_url, mirror, sizeof(mirror)) ) {
        /* Copy it to the internal data */
        free(urlset->preferred_site);
        urlset->preferred_site = safe_strdup(mirror);

        /* Write it out to disk */
        preferences_path("preferred_mirror.txt", mirror, sizeof(mirror));
        fp = fopen(mirror, "w");
        if ( fp ) {
            fprintf(fp, "%s\n", urlset->preferred_site);
            fclose(fp);
        } else {
            log(LOG_WARNING, _("Unable to write to %s\n"), mirror);
        }
    }
}

/* Reset the status of a set or URLs */
void reset_urlset(urlset *urlset)
{
    struct mirror_url *mirror;

    for ( mirror = urlset->list; mirror; mirror = mirror->next ) {
        mirror->status = URL_OK;
    }
    urlset->current = NULL;
    urlset->num_okay = urlset->num_mirrors;
}

/* Free a set of update URLs */
void free_urlset(urlset *urlset)
{
    struct mirror_url *freeable;

    while ( urlset->list ) {
        freeable = urlset->list;
        urlset->list = urlset->list->next;
        free(freeable->url);
        free(freeable);
    }
    free(urlset->preferred_site);
    free(urlset);
}
