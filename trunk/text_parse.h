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

/* These functions are just designed to strip HTML tags out of a file
   so the text can be parsed normally.
*/

struct text_fp;

extern void text_close(struct text_fp *textfp);

extern struct text_fp *text_open(const char *file);

extern char *text_line(char *line, int maxlen, struct text_fp *textfp);

extern int text_parsefield(struct text_fp *textfp, char *key, int keylen,
                                                char *value, int valuelen);
