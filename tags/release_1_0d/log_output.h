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

#ifndef _LOG_OUTPUT_H
#define _LOG_OUTPUT_H

#include <libintl.h>
#define _(String) gettext (String)
#define gettext_noop(String) (String)

enum {
    LOG_DEBUG = 0,
    LOG_VERBOSE,
    LOG_STATUS,
    LOG_NORMAL,
    LOG_WARNING,
    LOG_ERROR,
    LOG_NONE
};

extern void set_logging(int level);
extern int get_logging(void);
extern void log(int level, const char *fmt, ...);

#endif /* _LOG_OUTPUT_H */
