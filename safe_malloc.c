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

#include <stdlib.h>
#include <string.h>

#include "safe_malloc.h"
#include "log_output.h"


void *safe_malloc(size_t size)
{
    void *mem;

    mem = malloc(size);
    if ( ! mem ) {
        log(LOG_ERROR, "Out of memory, allocating %d bytes\n", size);
        abort();
    }
    return(mem);
}

void *safe_realloc(void *mem, size_t size)
{
    mem = realloc(mem, size);
    if ( ! mem ) {
        log(LOG_ERROR, "Out of memory, allocating %d bytes\n", size);
        abort();
    }
    return(mem);
}

void safe_free(void *mem)
{
    if ( mem ) {
        free(mem);
    }
}

char *safe_strdup(const char *string)
{
    char *newstring;

    newstring = safe_malloc(strlen(string)+1);
    strcpy(newstring, string);
    return(newstring);
}
