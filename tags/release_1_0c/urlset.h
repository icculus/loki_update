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

/* A set of URLs, an opaque pointer */
typedef struct url_bucket url_bucket_list;
typedef struct urlset {
    url_bucket_list *urls;
    url_bucket_list *current;
} urlset;


/* Create a set of update URLs */
extern urlset *create_urlset(void);

/* Add a URL to a set of update URLs */
extern void add_url(urlset *urlset, const char *url, int order);

/* Get the next URL to be tried for an update */
extern const char *get_next_url(urlset *urlset);

/* Free a set of update URLs */
extern void free_urlset(urlset *urlset);
