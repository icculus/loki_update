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
#include <stdarg.h>

#include "log_output.h"

static int log_level = LOG_NORMAL;

void set_logging(int level)
{
    log_level = level;
}

int get_logging(void)
{
    return(log_level);
}

void log(int level, const char *fmt, ...)
{
    if ( level >= log_level ) {
        va_list ap;

        va_start(ap, fmt);
        switch (level) {
            case LOG_ERROR:
                fprintf(stdout, _("ERROR: "));
                break;
            case LOG_WARNING:
                fprintf(stdout, _("WARNING: "));
                break;
            default:
                break;
        }
        vfprintf(stdout, fmt, ap);
        va_end(ap);
        fflush(stdout);
    }
}
