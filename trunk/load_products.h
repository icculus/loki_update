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

extern void load_product_list(const char *wanted);

extern int get_num_products(void);
extern const char *get_first_product(void);
extern const char *get_next_product(void);

extern int is_valid_product(const char *product);

extern void set_override_url(const char *update_url);
extern void set_product_root(const char *product, const char *root);
extern void set_product_url(const char *product, const char *url);

extern const char *get_product_version(const char *product);
extern const char *get_product_description(const char *product);
extern const char *get_product_root(const char *product);
extern const char *get_product_url(const char *product);
extern const char *get_default_component(const char *product);

extern void free_product_list(void);

extern int is_product_path(const char *product);
extern const char *link_product_path(const char *path);
