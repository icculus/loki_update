
/* Given a product and version, return a set of available patches */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "patchset.h"
#include "arch.h"
#include "log_output.h"

/* Construct a tree of available versions */

static void free_patch(patch *patch)
{
    if ( patch->description ) {
        free(patch->description);
    }
    if ( patch->component ) {
        free(patch->component);
    }
    if ( patch->version ) {
        free(patch->version);
    }
    if ( patch->applies ) {
        free(patch->applies);
    }
    if ( patch->url ) {
        free(patch->url);
    }
    free(patch);
}

static void free_pathlist(patch_path *pathlist)
{
    patch *patch;
    patch_path *path;

    while ( pathlist ) {
        path = pathlist;
        pathlist = path->next;
        while ( path->patches ) {
            patch = path->patches;
            path->patches = patch->next; 
            free_patch(patch);
        }
        free(path);
    }
}

void free_patchset(struct patchset *patchset)
{
    if ( patchset ) {
        if ( patchset->product ) {
            loki_closeproduct(patchset->product);
        }
        if ( patchset->paths ) {
            free_pathlist(patchset->paths);
        }
        if ( patchset->patchlist ) {
            patch *patch;
            int i;
            for ( i=0; i<patchset->num_patches; ++i ) {
                patch = patchset->patchlist[i];
                free_patch(patch);
            }
            free(patchset->patchlist);
        }
        free(patchset);
    }
}

patchset *create_patchset(const char *product)
{
    struct patchset *patchset;

    patchset = (struct patchset *)malloc(sizeof *patchset);
    if ( patchset ) {
        patchset->product = loki_openproduct(product);
        patchset->product_name = loki_getinfo_product(patchset->product)->name;
        patchset->paths = NULL;
        patchset->num_patches = 0;
        patchset->patchlist = NULL;
        if ( ! patchset->product ) {
            log(LOG_ERROR, "Unable to find %s in registry\n", product);
            free_patchset(patchset);
            patchset = (struct patchset *)0;
        }
    } else {
        log(LOG_ERROR, "Out of memory\n");
    }
    return patchset;
}

static const char *copy_word(const char *string, char *word, int wordlen)
{
    char *bufp;

    if ( *string == '\0' ) {
        return NULL;
    }
    while ( isspace(*string) ) {
        ++string;
    }
    bufp = word;
    while ( *string && (*string != ',') ) {
        if ( --wordlen > 0 ) {
            *bufp++ = *string++;
        }
    }
    if ( *string ) {
        ++string;
    }
    while ( (bufp > word) && isspace(*(bufp-1)) ) {
        --bufp;
    }
    if ( bufp ) {
        *bufp = '\0';
    }
    return string;
}

/*
    Version:
    Architecture:
    Applies to:
    Installed Size:
    URL:
*/
int add_patch(const char *product,
              const char *component,
              const char *version,
              const char *arch,
              const char *applies,
              const char *url,
              struct patchset *patchset)
{
    int matched_arch;
    const char *next;
    char word[128];
    char description[256];
    patch *the_patch;

    /* It's legal to have no component, which means the default component */
    if ( component ) {
        snprintf(description, sizeof(description), "%s %s", component, version);
    } else {
        product_component_t *product_component;

        product_component = loki_getdefault_component(patchset->product);
        if ( product_component ) {
            component = loki_getname_component(product_component);
        } else {
            component = "";
        }
        snprintf(description, sizeof(description), "%s %s", product, version);
    }
    log(LOG_DEBUG, "Potential patch:\n");
    log(LOG_DEBUG, "\tProduct: %s\n", product);
    log(LOG_DEBUG, "\tComponent: %s\n", component);
    log(LOG_DEBUG, "\tVersion: %s\n", version);
    log(LOG_DEBUG, "\tArchitecture: %s\n", arch);
    log(LOG_DEBUG, "\tURL: %s\n", url);

    if (strcasecmp(product,loki_getinfo_product(patchset->product)->name) != 0){
        log(LOG_DEBUG, "Patch for different product\n");
        return(0);
    }

    /* Parse into individual arch tokens and check them */
    matched_arch = 0;
    for ( next=copy_word(arch, word, sizeof(word));
          next; 
          next=copy_word(next, word, sizeof(word)) ) {
        if ( strcasecmp(word, detect_arch()) == 0 ) {
            matched_arch = 1;
            break;
        }
    }
    if ( ! matched_arch ) {
        log(LOG_DEBUG, "Patch for different architecture\n");
        return(0);
    }

    /* Create the patch */
    the_patch = (patch *)malloc(sizeof *the_patch);
    if ( ! the_patch ) {
        log(LOG_ERROR, "Out of memory\n");
        return(-1);
    }
    the_patch->next = NULL;
    if ( loki_find_component(patchset->product, component) ) {
        the_patch->type = TYPE_PATCH;
    } else {
        the_patch->type = TYPE_ADDON;
    }
    the_patch->description = strdup(description);
    the_patch->component = strdup(component);
    the_patch->version = strdup(version);
    the_patch->applies = strdup(applies);
    the_patch->url = strdup(url);
    the_patch->selected = 0;
    if ( ! the_patch->description ||
         ! the_patch->component || ! the_patch->version ||
         ! the_patch->applies || ! the_patch->url ) {
        free_patch(the_patch);
        log(LOG_ERROR, "Out of memory\n");
        return(-1);
    }

    /* Add it to our list */
    patchset->patchlist = (patch **)realloc(patchset->patchlist,
                                (patchset->num_patches+1)*sizeof(patch*));
    patchset->patchlist[patchset->num_patches++] = the_patch;
    return(0);
}

static int patch_applies_product(struct patch *patch, product_t *product)
{
    product_component_t *component;
    const char *next;
    const char *version;
    char applies[128];

    /* Parse into individual version tokens and check them */
    for ( component=loki_getfirst_component(product);
          component;
          component=loki_getnext_component(component) ) {
        if ( strcmp(loki_getname_component(component),patch->component) == 0 ) {
            version = loki_getversion_component(component);
            for ( next=copy_word(patch->applies, applies, sizeof(applies));
                  next; 
                  next=copy_word(next, applies, sizeof(applies)) ) {
                if ( strcmp(applies, version) == 0 ) {
                    return(1);
                }
            }
        }
    }
    return(0);
}

static int patch_applies_patch(struct patch *patch, struct patch *leaf)
{
    const char *next;
    char applies[128];

    /* If the components don't match, then it definitely doesn't apply */
    if ( strcmp(patch->component, leaf->component) == 0 ) {
        /* Parse into individual version tokens and check them */
        for ( next=copy_word(patch->applies, applies, sizeof(applies));
              next; 
              next=copy_word(next, applies, sizeof(applies)) ) {
            if ( strcmp(applies, leaf->version) == 0 ) {
                return(1);
            }
        }
    }
    return(0);
}

/* Add a patch to a given patch path, or create one if necessary */
static int add_patch_path(patch *patch, patch_path *path, patchset *patchset)
{
    if ( ! path ) {
        path = (patch_path *)malloc(sizeof *path);
        if ( ! path ) {
            return(-1);
        }
        path->next = patchset->paths;
        path->leaf = NULL;
        path->patches = NULL;
        patchset->paths = path;
    }

    if ( ! path->leaf ) {
        path->patches = patch;
        path->leaf = patch;
    } else {
        path->leaf->next = patch;
        path->leaf = patch;
    }
    return(0);
}

int path_length(patch_path *path, const char *version)
{
    int length;
    patch *patch;

    length = 0;
    for ( patch=path->patches; patch; patch=patch->next ) {
        ++length;
        if ( version && (strcmp(patch->version, version) == 0) ) {
            break;
        }
    }
    if ( version && !patch ) {
        /* Version not found in path */
        length = 0;
    }
    return length;
}

/* Generate trees of patch versions, trimming out those that don't apply */
void make_tree(patchset *patchset)
{
    int i, applied;
    int patch_used;
    patch *patch;
    patch_path *path;


    /* If we have no patches, nothing to do */
    if ( ! patchset->patchlist ) {
        return;
    }

    /* Add all patches that apply to leaf nodes */
    do {
        applied = 0;
        for ( i=patchset->num_patches-1; i >= 0; --i ) {
            patch_used = 0;
            patch = patchset->patchlist[i];
            if ( patch_applies_product(patch, patchset->product) ) {
                ++patch_used;
                add_patch_path(patch, 0, patchset);
            } else {
                for ( path=patchset->paths; path; path=path->next ) {
                    if ( patch_applies_patch(patch, path->leaf) ) {
                        ++patch_used;
                        add_patch_path(patch, path, patchset);
                    }
                }
            }
            if ( patch_used ) {
                memcpy(&patchset->patchlist[i], &patchset->patchlist[i+1],
                       (patchset->num_patches-i)*sizeof(*patchset->patchlist));
                --patchset->num_patches;
                ++applied;
            }
        }
    } while ( applied );

    /* Free all unused patches */
    for ( i=0; i<patchset->num_patches; ++i ) {
        patch = patchset->patchlist[i];
        log(LOG_DEBUG, "Freeing unused patch: %s\n", patch->version);
        free_patch(patch);
    }
    free(patchset->patchlist);
    patchset->patchlist = NULL;
}


/* See if a patch is a non-leaf portion of a patchset paths */
static int is_nonleaf(patch *the_patch, patchset *patchset)
{
    patch *branch;
    patch_path *path;

    for ( path=patchset->paths; path; path=path->next ) {
        for ( branch=path->patches; branch != path->leaf; branch=branch->next ){
            if ( strcmp(the_patch->version, branch->version) == 0 ) {
                return(1);
            }
        }
    }
    return(0);
}

static int is_longpath(patch_path *path, patchset *patchset)
{
    patch_path *branch;
    int length;

    length = path_length(path, NULL);
    for ( branch=patchset->paths; branch; branch=branch->next ) {
        /* Only compare lengths if this path has same leaf as other path */
        if ( (branch == path) ||
             (strcmp(path->leaf->version, branch->leaf->version) != 0) ) {
            continue;
        }
        if ( path_length(branch, NULL) <= length ) {
            return(1);
        }
    }
    return(0);
}

void collapse_tree(patchset *patchset)
{
    patch_path *path;
    patch_path *last;
    patch_path *good_paths, *bad_paths;

    /* Remove all paths that are subsets of other paths */
    good_paths = NULL;
    bad_paths = NULL;
    path = patchset->paths;
    while ( path ) {
        last = path;
        path = path->next;
        if ( is_nonleaf(last->leaf, patchset) ) {
            last->next = bad_paths;
            bad_paths = last;
        } else {
            last->next = good_paths;
            good_paths = last;
        }
    }
    patchset->paths = good_paths;

    /* Remove all paths that are longer duplicates of other paths */
    good_paths = NULL;
    path = patchset->paths;
    while ( path ) {
        last = path;
        path = path->next;
        if ( is_longpath(last, patchset) ) {
            last->next = bad_paths;
            bad_paths = last;
        } else {
            last->next = good_paths;
            good_paths = last;
        }
    }
    patchset->paths = good_paths;

    /* What we have left is a list of valid short paths */
    free_pathlist(bad_paths);
}

/* Autoselect the paths which do not involve installing new components */
void autoselect_patches(patchset *patchset)
{
    patch *patch;
    patch_path *path;

    path = patchset->paths;
    while ( path ) {
        patch = path->patches;
        while ( patch ) {
            if ( patch->type == TYPE_PATCH ) {
                patch->selected = 1;
            }
            patch = patch->next;
        }
        path = path->next;
    }
}
