
#ifndef _patchset_h
#define _patchset_h

#include "setupdb.h"

typedef struct patch {
    char *component;
    char *version;
    char *applies;
    char *url;
    struct patch *next;
} patch;

typedef struct patch_path {
    patch *leaf;
    patch *patches;
    struct patch_path *next;
} patch_path;

typedef struct patchset {
    product_t *product;

    patch_path *paths;

    int num_patches;
    struct patch **patchlist;
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
                     const char *applies,
                     const char *url,
                     struct patchset *patchset);

/* Generate trees of patch versions, trimming out those that don't apply */
extern void make_tree(patchset *patchset);
extern void collapse_tree(patchset *patchset);

/* Check the length of a path to a particular version */
extern int path_length(patch_path *path, const char *version);

#endif /* _patchset_h */
