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

#include <limits.h>

/* A set of URLs used as mirror locations */
struct mirror_url {
    char *url;
    enum url_status {
        URL_OK,
        URL_USED,
        URL_FAILED
    } status;
    void *data;
    struct mirror_url *next;
};
typedef struct {
    int num_mirrors;
    int num_okay;
    struct mirror_url *list;
    struct mirror_url *current;
    char full_url[PATH_MAX];
    char *preferred_site;
} urlset;

/* Create a set of mirror URLs */
extern urlset *create_urlset(void);

/* Get the host from a URL, returning 1 or 0 if there is no host */
extern int get_url_host(const char *url, char *dst, int maxlen);

/* Add a URL to a set of update URLs */
extern void add_url(urlset *urlset, const char *url);

/* Randomize the order of the mirrors */
extern void randomize_urls(urlset *urlset);

/* Set and save the preferred URL */
extern void set_preferred_url(urlset *urlset, const char *url);
extern void save_preferred_url(urlset *urlset);

/* Get the next URL to be tried for an update */
extern const char *get_next_url(urlset *urlset, const char *file);

/* Set the status of the current URL */
extern void set_url_status(urlset *urlset, enum url_status status);

/* Reset the status of a set or URLs */
extern void reset_urlset(urlset *urlset);

/* Free a set of update URLs */
extern void free_urlset(urlset *urlset);
