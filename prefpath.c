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

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>

#include "mkdirhier.h"
#include "prefpath.h"

/* Return a path in the preferences directory.
   Directories are created as needed to generate the path.
 */
void preferences_path(const char *file, char *dst, int maxlen)
{
    static const char *home = NULL;

    /* First look up the user's home directory in the password file,
       based on effective user id.
     */
    if ( ! home ) {
        struct passwd *pwent;

        pwent = getpwuid(geteuid());
        if ( pwent ) {
            /* Small memory leak, don't worry about it */
            home = strdup(pwent->pw_dir);
        }
    }

    /* We didn't find the user in the password file?
       Something wierd is going on, but use the HOME environment anyway.
     */
    if ( ! home ) {
        home = getenv("HOME");
    }

    /* Uh oh, this system is probably hosed, write to / if we can.
     */
    if ( ! home ) {
        home = "";
    }

    /* Assemble the path and create any necessary directories */
    snprintf(dst, maxlen, "%s/.loki/loki_update/%s", home, file);
    mkdirhier(dst);
}
