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

#include <string.h>
#include <limits.h>

#include "url_paths.h"

static char working_path[PATH_MAX];

void set_working_path(const char *cwd)
{
    strncpy(working_path, cwd, sizeof(working_path));
}

const char *get_working_path(void)
{
    return(working_path);
}

/* Compose a full URL from a base and a relative URL */
char *compose_url(const char *base, const char *url, char *full, int maxlen)
{
    char *bufp;

    bufp = strstr(url, "://");
    if ( base && ! bufp && (*url != '/') ) {
        bufp = strstr(base, "://");
        if ( working_path[0] && ! bufp && (*base != '/') ) {
            strncpy(full, working_path, maxlen-1);
            strcat(full, "/");
            strncat(full, base, maxlen-strlen(full));
        } else {
            strncpy(full, base, maxlen);
        }
        bufp = strrchr(full, '/');
        if ( bufp++ ) {
            *bufp = '\0';
        }
        strncat(full, url, maxlen-strlen(full));
    } else {
        bufp = strstr(url, "://");
        if ( working_path[0] && ! bufp && (*url != '/') ) {
            strncpy(full, working_path, maxlen-1);
            strcat(full, "/");
            strncat(full, url, maxlen-strlen(full));
        } else {
            strncpy(full, url, maxlen);
        }
    }
    return(full);
}

