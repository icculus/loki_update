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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "mkdirhier.h"

/* Create the directories in the heirarchy above this path, if necessary */
int mkdirhier(const char *path)
{
	int retval;
	char *bufp, *new_path;
	struct stat sb;

	retval = 0;
	if ( path && *path ) {
		new_path = strdup(path);
		for ( bufp=new_path+1; *bufp; ++bufp ) {
			if ( *bufp == '/' ) {
				*bufp = '\0';
				if ( stat(new_path, &sb) < 0 ) {
					retval = mkdir(new_path, 0755);
				}
				*bufp = '/';
			}
		}
		free(new_path);
	}
	return(retval);
}
