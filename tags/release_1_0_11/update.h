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

/* Header file for the update function and the download update callback */

#ifndef _UPDATE_H
#define _UPDATE_H

#include "log_output.h"

typedef int (*update_callback)(int status_level, const char *status,
                               float percentage, int size, int total,
                               float rate, void *udata);

extern void update_message(int level, const char *message,
                           update_callback update, void *udata);

extern int perform_update(const char *update_file, const char *install_path,
                          update_callback update, void *udata);

#endif /* _UPDATE_H */
