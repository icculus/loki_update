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

#ifndef _patchset_h
#define _patchset_h

#include "urlset.h"

/* Forward declarations */
struct patchset;
struct patch;
struct patch_path;

/* This is the version node that is traversed for UI display */
typedef struct version_node {
    char *component;                /* The component for this node */
    char *version;                  /* The key data for this node */
    char *description;              /* The description of this node */
    char *note;                     /* A note for the user about this node */
    int toggled;                    /* True if on the version path selected */
    int invisible;                  /* Never selectable, true for root nodes */
    int depth;                      /* Depth in the node tree */
    int top_root;                   /* True if this is the toplevel root */
    struct version_node *root;      /* A pointer to the root for this node */
    struct version_node *child;     /* Used by the follow-edge nodes */
    struct version_node *sibling;   /* Other flavors of this version */
    int num_adjacent;               /* The number of adjacent nodes */
    struct version_node **adjacent; /* Adjacent nodes (via patches) */
    struct patch_path *shortest_path; /* Shortest path from root node */
    void *udata;                    /* Used to store UI information */

    /* Information used in the shortest-path algorithm */
    int index;
    struct version_node *next;
    struct version_node *prev;
    struct version_node *last;

    /* Information stored in the root node about selected node */
    struct version_node *selected;
} version_node;

typedef struct patch_path {
    version_node *src;
    version_node *dst;
    struct patch *patch;
    struct patch_path *next;
} patch_path;

typedef struct patch {
    struct patchset *patchset;
    char *description;
    int size;
    urlset *urls;
    struct version_node *node;
    int refcount;
    int installed;
    int num_apply;
    struct version_node **apply;
    struct patch *next;
} patch;

typedef struct patchset {
    const char *product_name;

    version_node *root;
    patch *patches;

    struct patchset *next;

    /* Temporary memory used by the shortest path algorithm */
    int *seen;
    int *dist;
    version_node **parent;
    version_node **fringe;
} patchset;


extern patchset *create_patchset(const char *product);
extern void free_patchset(struct patchset *patchset);

/*
    Version:
    Architecture:
    Applies to:
    Installed Size:
    URL:
*/
extern int add_patch(const char *product,
                     const char *component,
                     const char *version,
                     const char *arch,
                     const char *libc,
                     const char *applies,
                     const char *note,
                     const char *size,
                     urlset *urls,
                     struct patchset *patchset);

/* Generate valid patch paths, trimming out versions that don't apply */
extern void calculate_paths(patchset *patchset);

/* Select a particular version node and set toggled state */
extern void select_node(version_node *selected_node, int selected);

/* Find out how much bandwidth all selected updates will take */
extern int selected_size(patchset *patchset);

/* Select the main branch of patches for the installed components */
extern void autoselect_patches(patchset *patchset);

#endif /* _patchset_h */
