
/* Given a product and version, return a set of available patches */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "safe_malloc.h"
#include "patchset.h"
#include "arch.h"
#include "log_output.h"


static const char *get_version_extension(version_node *node)
{
    const char *ext;

    ext = node->version;
    while ( isalnum(*ext) || (*ext == '.') ) {
        ++ext;
    }
    return(ext);
}

static void get_word(const char **string, char *word, int maxlen)
{
    while ( (maxlen > 0) && **string && isalpha(**string) ) {
        *word = **string;
        ++word;
        ++*string;
        --maxlen;
    }
    *word = '\0';
}

static int get_num(const char **string)
{
    int num;

    num = atoi(*string);
    while ( isdigit(**string) ) {
        ++*string;
    }
    return(num);
}

static int newer_version(const char *version1, const char *version2)
{
    int newer;
    int num1, num2;
    char base1[128], ext1[128];
    char base2[128], ext2[128];
    char word1[32], word2[32];

    /* Compare sequences of numbers and letters in the base versions */
    newer = 0;
    loki_split_version(version1, base1, sizeof(base1), ext1, sizeof(ext1));
    version1 = base1;
    loki_split_version(version2, base2, sizeof(base2), ext2, sizeof(ext2));
    version2 = base2;
    while ( !newer && (*version1 && *version2) &&
            ((isdigit(*version1) && isdigit(*version2)) ||
             (isalpha(*version1) && isalpha(*version2))) ) {
        if ( isdigit(*version1) ) {
            num1 = get_num(&version1);
            num2 = get_num(&version2);
            if ( num1 > num2 ) {
                newer = 1;
            }
        } else {
            get_word(&version1, word1, sizeof(word1));
            get_word(&version2, word2, sizeof(word2));
            if ( strcasecmp(word1, word2) > 0 ) {
                newer = 1;
            }
        }
        if ( *version1 == '.' ) {
            ++version1;
        }
        if ( *version2 == '.' ) {
            ++version2;
        }
    }
    if ( isalpha(*version1) && !isalpha(*version2) ) {
        newer = 1;
    }
    return(newer);
}

static void free_patch_path(patch_path *path)
{
    if ( path ) {
        free_patch_path(path->next);
        free(path);
    }
}

static void free_version_node(version_node *node)
{
    if ( node ) {
        free(node->component);
        free(node->version);
        free(node->description);
        free_version_node(node->child);
        free_version_node(node->sibling);
        free_patch_path(node->shortest_path);
        safe_free(node->adjacent);
        if ( node->prev ) {
            node->prev->next = node->next;
        }
        if ( node->next ) {
            node->next->prev = node->prev;
        } else {
            node->root->last = node->prev;
        }
        free(node);
    }
}

static version_node *create_version_node(version_node *root,
                                         const char *component,
                                         const char *version,
                                         const char *description)
{
    version_node *node;

    /* Create and initialize the node */
    node = (version_node *)safe_malloc(sizeof *node);
    node->component = safe_strdup(component);
    node->version = safe_strdup(version);
    node->description = safe_strdup(description);
    node->selected = 0;
    node->toggled = 0;
    node->invisible = 0;
    node->depth = 0;
    if ( root ) {
        node->root = root;
    } else {
        node->root = node;
    }
    node->child = NULL;
    node->sibling = NULL;
    node->num_adjacent = 0;
    node->adjacent = NULL;
    node->shortest_path = NULL;
    node->udata = NULL;
    node->index = 0;

    /* Add the node to the list of patches for shortest-path traversal */
    if ( root ) {
        node->prev = root->last;
        root->last->next = node;
        root->last = node;
    } else {
        node->prev = NULL;
        node->last = node;
    }
    node->next = NULL;

    /* We're ready to go! */
    return(node);
}

static version_node *get_version_node(version_node *root,
                                      const char *component,
                                      const char *version,
                                      const char *description)
{
    version_node *node, *parent;

    /* Find the correct component root to use for this version */
    while ( root ) {
        if ( strcasecmp(root->component, component) == 0 ) {
            break;
        }
        root = root->sibling;
    }
    if ( ! root ) {
        /* This is a new component add-on */
        // TODO
        return(NULL);
    }

    /* If this is older than the root version, don't use it */
    if ( newer_version(root->version, version) ) {
        return(NULL);
    }

    /* Now see if this version node is already available */
    parent = root;
    for ( node = root; node; node = node->child ) {
        /* If this is the exact node we're looking for.. */
        if ( strcasecmp(node->version, version) == 0 ) {
            break;
        }

        /* If the current node is newer than us, add us as parent */
        if ( newer_version(node->version, version) ) {
            node = NULL;
            break;
        }

        /* If we are not newer than the current node, we are a sibling */
        if ( !newer_version(version, node->version) && (node != root) ) {
            parent = node;
            for ( node=node->sibling; node; node=node->sibling ) {
                /* If this is the exact node we're looking for.. */
                if ( strcasecmp(node->version, version) == 0 ) {
                    break;
                }
                parent = node;
            }
            /* We need to add ourselves as a new sibling */
            if ( ! node ) {
                node = create_version_node(root,
                                           component, version, description);
                node->sibling = parent->sibling;
                parent->sibling = node;
            }
            break;
        }

        /* Newer than current node, keep traversing the tree */
        parent = node;
    }

    /* If we need to insert ourselves here, do so */
    if ( ! node ) {
        node = create_version_node(root, component, version, description);
        node->child = parent->child;
        parent->child = node;
    }
    return(node);
}

static void add_adjacent_node(version_node *node, patch *patch)
{
    node->adjacent = (version_node **)safe_realloc(node->adjacent,
        (node->num_adjacent+1)*(sizeof *node->adjacent));
    node->adjacent[node->num_adjacent++] = patch->node;

    patch->apply = (version_node **)safe_realloc(patch->apply,
        (patch->num_apply+1)*(sizeof *patch->apply));
    patch->apply[patch->num_apply++] = node;
}

/* Construct a tree of available versions */

static void free_patch(patch *patch)
{
    if ( patch ) {
        free(patch->description);
        free(patch->url);
        safe_free(patch->apply);
        free_patch(patch->next);
        free(patch);
    }
}

void free_patchset(struct patchset *patchset)
{
    if ( patchset ) {
        free_patchset(patchset->next);
        if ( patchset->product ) {
            loki_closeproduct(patchset->product);
        }
        free_version_node(patchset->root);
        free_patch(patchset->patches);
        free(patchset);
    }
}

patchset *create_patchset(const char *product)
{
    struct patchset *patchset;
    product_component_t *component;

    patchset = (struct patchset *)safe_malloc(sizeof *patchset);
    patchset->product = loki_openproduct(product);
    if ( ! patchset->product ) {
        log(LOG_ERROR, "Unable to find %s in registry\n", product);
        free(patchset);
        return(NULL);
    }
    component = loki_getdefault_component(patchset->product);
    if ( ! component ) {
        log(LOG_ERROR, "No default component for %s\n", product);
        loki_closeproduct(patchset->product);
        free(patchset);
        return(NULL);
    }
    patchset->product_name = loki_getinfo_product(patchset->product)->name;
    patchset->root = create_version_node(NULL,
                                         loki_getname_component(component),
                                         loki_getversion_component(component),
                                         "Root node");
    patchset->root->invisible = 1;
    patchset->patches = NULL;
    patchset->next = NULL;

    /* We're ready to go */
    return patchset;
}

static const char *default_component(product_t *product)
{
    product_component_t *product_component;
    const char *component;

    product_component = loki_getdefault_component(product);
    if ( product_component ) {
        component = loki_getname_component(product_component);
    } else {
        component = "";
    }
    return component;
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

static int legal_version_combination(const char *version1,
                                     const char *version2)
{
    int legal;
    char base1[1024], ext1[1024];
    char base2[1024], ext2[1024];

    /* To keep things relatively simple, interface and implementation
       wise, we'll only allow linear or lateral version changes:
        Same base version, different flavor, okay
        Different base version, same flavor, okay
        Otherwise we're doing a diagonal upgrade, not allowed.
    */
    loki_split_version(version1, base1, sizeof(base1), ext1, sizeof(ext1));
    loki_split_version(version2, base2, sizeof(base2), ext2, sizeof(ext2));
    if ( ((strcmp(base1, base2) == 0) && (strcmp(ext1, ext2) != 0)) ||
         ((strcmp(base1, base2) != 0) && (strcmp(ext1, ext2) == 0)) ) {
        legal = 1;
    } else {
        legal = 0;
    }
    return(legal);
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
    const char *saved_component;
    int matched_arch;
    const char *next;
    char word[128];
    char description[1024];
    patch *patch;
    version_node *node;

    /* It's legal to have no component, which means the default component */
    saved_component = component;
    if ( component ) {
        snprintf(description, sizeof(description), "%s %s", component, version);
    } else {
        component = default_component(patchset->product);
        snprintf(description, sizeof(description), "Patch %s", version);
    }
    log(LOG_DEBUG, "Potential patch:\n");
    log(LOG_DEBUG, "\tProduct: %s\n", product);
    log(LOG_DEBUG, "\tComponent: %s\n", component);
    log(LOG_DEBUG, "\tVersion: %s\n", version);
    log(LOG_DEBUG, "\tArchitecture: %s\n", arch);
    log(LOG_DEBUG, "\tURL: %s\n", url);

    if ( strcasecmp(product, patchset->product_name) != 0 ) {
        log(LOG_DEBUG, "Patch for different product, dropping\n");
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
        log(LOG_DEBUG, "Patch for different architecture, dropping\n");
        return(0);
    }

    /* Create the version_node */
    node = get_version_node(patchset->root, component, version, description);
    if ( ! node ) {
        log(LOG_DEBUG, "Patch obsolete by installed version, dropping\n");
        return(0);
    }

    /* Create the patch */
    patch = (struct patch *)safe_malloc(sizeof *patch);
    patch->patchset = patchset;
    patch->description = safe_strdup(description);
    patch->url = safe_strdup(url);
    patch->node = node;
    patch->installed = 0;
    patch->num_apply = 0;
    patch->apply = NULL;
    patch->next = NULL;

    /* Link it as adjacent to the versions it applies to */
    for ( next=copy_word(applies, word, sizeof(word));
          next; 
          next=copy_word(next, word, sizeof(word)) ) {
        if ( component == default_component(patchset->product) ) {
            snprintf(description, sizeof(description), "Patch %s", word);
        } else {
            snprintf(description, sizeof(description), "%s %s", component,word);
        }
        node = get_version_node(patchset->root, component, word, description);
        if ( ! node ) {
            /* This is an obsolete version, ignore it */
            continue;
        }
        if ( legal_version_combination(node->version, patch->node->version) ) {
            add_adjacent_node(node, patch);
        } else {
            log(LOG_DEBUG,
                "Version combination %s and %s isn't legal, dropping\n",
                node->description, patch->node->description);
        }
    }

    /* Add it to our list of patches */
    patch->next = patchset->patches;
    patchset->patches = patch;

    /* We're done */
    return(0);
}

static patch *find_linking_patch(patchset *patchset,
                                 version_node *src, version_node *dst)
{
    patch *patch;
    int i;

    for ( patch = patchset->patches; patch; patch = patch->next ) {
        if ( patch->node == dst ) {
            for ( i = patch->num_apply-1; i >= 0; --i ) {
                if ( src == patch->apply[i] ) {
                    break;
                }
            }
            if ( i >= 0 ) {
                break;
            }
        }
    }
    return(patch);
}

/* Find the shortest path from root to leaf, using Dijkstra's algorithm */
static patch_path *find_shortest_path(version_node *root,
                                       version_node *leaf,
                                       patchset *patchset)
{
    patch_path *path, *newpath;
    version_node *node, **parent, **fringe;
    int i, a;
    int *seen;
    int *dist;
    int stuck;
    int num_fringes;

    /* All nodes except root are unseen */
    seen = patchset->seen;
    dist = patchset->dist;
    parent = patchset->parent;
    fringe = patchset->fringe;
    for ( node=root; node; node=node->next ) {
        a = node->index;
        if ( node == root ) {
            seen[a] = 2;
            dist[a] = 0;
            parent[a] = NULL;
        } else {
            seen[a] = 0;
        }
    }
    num_fringes = 0;

    stuck = 0;
    node = root;
    while ( (node != leaf) && !stuck ) {
        for ( i=0; i<node->num_adjacent; ++i ) {
            a = node->adjacent[i]->index;
            if ( (seen[a] == 1) && ((dist[node->index]+1) < dist[a]) ) {
                parent[a] = node;
            }
            if ( seen[a] == 0 ) {
                seen[a] = 1;
                fringe[num_fringes++] = node->adjacent[i];
                parent[a] = node;
                dist[a] = dist[node->index]+1;
            }
        }
        if ( num_fringes > 0 ) {
            int mindist;
            int shortest;

            mindist = INT_MAX;
            for ( i=0; i<num_fringes; ++i ) {
                if ( dist[fringe[i]->index] < mindist ) {
                    mindist = dist[fringe[i]->index];
                    shortest = i;
                }
            }
            node = fringe[shortest];
            memcpy(&fringe[shortest], &fringe[shortest+1],
                   (num_fringes-shortest)*(sizeof *fringe));
            --num_fringes;
        } else {
            stuck = 1;
        }
    }

    path = NULL;
    if ( node == leaf ) {
        while ( node != root ) {
            newpath = (patch_path *)safe_malloc(sizeof *newpath);
            newpath->next = path;
            newpath->src = parent[node->index];
            newpath->dst = node;
            newpath->patch = find_linking_patch(patchset,
                                                newpath->src, newpath->dst);
            newpath->next = path;
            path = newpath;
            node = newpath->src;
        }
    }
    return(path);
}

static void trim_unconnected_nodes(version_node *trunk_prev,
                                   version_node *trunk)
{
    version_node *node, *next, *prev;

    while ( trunk ) {
        /* Trim the branch */
        for ( prev=trunk, next=trunk->sibling; next; ) {
            node = next;
            next = next->sibling;
            if ( node->shortest_path ) {
                prev = node;
            } else {
                prev->sibling = next;
                node->sibling = NULL;
                log(LOG_DEBUG, "%s has no patch path, trimming\n", 
                    node->description);
                free_version_node(node);
            }
        }
        /* Trim this node, if necessary */
        node = trunk;
        trunk = trunk->child;
        if ( node->shortest_path ) {
            trunk_prev = node;
        } else {
            if ( node->sibling ) {
                node->sibling->child = node->child;
                trunk_prev->child = node->sibling;
                trunk_prev = trunk_prev->child;
                node->sibling = NULL;
            } else {
                trunk_prev->child = node->child;
            }
            node->child = NULL;
            log(LOG_DEBUG, "%s has no patch path, trimming\n", 
                node->description);
            free_version_node(node);
        }
    }
}

/* Generate valid patch paths, trimming out versions that don't apply */
void calculate_paths(patchset *patchset)
{
    version_node *node, *trunk, *root;
    int depth;
    int num_nodes;

    log(LOG_DEBUG, "Calculating patch paths for %s\n", patchset->product_name);

    /* Allocate memory for the shortest path algorithm */
    num_nodes = 0;
    for ( node = patchset->root; node; node = node->next ) {
        node->index = num_nodes++;
    }
    if ( num_nodes == 0 ) {
        /* Nothing to do, return */
        return;
    }
    patchset->seen = (int *)safe_malloc(num_nodes*(sizeof *patchset->seen));
    patchset->dist = (int *)safe_malloc(num_nodes*(sizeof *patchset->dist));
    patchset->parent = (version_node **)safe_malloc(num_nodes*(sizeof *patchset->parent));
    patchset->fringe = (version_node **)safe_malloc(num_nodes*(sizeof *patchset->fringe));

    root = patchset->root;
    while ( root ) {
        /* For all the nodes in the tree, generate a path from the root to it */
        depth = 0;
        for ( trunk=root->child; trunk; trunk=trunk->child ) {
            for ( node=trunk; node; node=node->sibling ) {
                node->depth = depth;
                node->shortest_path = find_shortest_path(root, node, patchset);
            }
            ++depth;
        }
        /* Trim all the nodes that don't have a path to the root */
        trim_unconnected_nodes(root, root->child);

        root = root->sibling;
    }

    /* Free shortest path memory */
    free(patchset->seen);
    free(patchset->dist);
    free(patchset->parent);
    free(patchset->fringe);
}

/* Select a particular version node and set toggled state */
void select_node(version_node *selected_node, int selected)
{
    version_node *node;

    /* Protect against bad parameters */
    if ( ! selected_node ) {
        return;
    }

    /* Select this node */
    if ( selected ) {
        selected_node->root->selected = selected_node;
    } else {
        selected_node->root->selected = NULL;
    }

    /* First clear the toggle state for all the nodes */
    for ( node=selected_node->root; node; node=node->next ) {
        node->toggled = 0;
    }

    /* Toggle on each node in the patch path, if selected */
    if ( selected ) {
        /* Now enable toggle state for earlier nodes with our extension */
        for ( node=selected_node->root; node; node=node->next ) {
            if ( node->depth < selected_node->depth ) {
                if ( strcasecmp(get_version_extension(selected_node),
                                get_version_extension(node)) == 0 ) {
                    node->toggled = 1;
                }
            }
        }
        selected_node->toggled = 1;
    } else {
        version_node *trunk;
        version_node *next;
        /* Nothing is selected, select the previous patch in our tree that
           matches our version extension.
         */
        next = NULL;
        for ( trunk=selected_node->root->child; trunk; trunk=trunk->child ) {
            if ( trunk->depth == selected_node->depth ) {
                break;
            }
            for ( node = trunk; node; node = node->sibling ) {
                if ( strcasecmp(get_version_extension(selected_node),
                                get_version_extension(node)) == 0 ) {
                    next = node;
                }
            }
        }
        select_node(next, 1);
    }
}

/* Select the main branch of patches for the installed components */
void autoselect_patches(patchset *patchset)
{
    version_node *node, *root;

    for ( root = patchset->root; root; root = root->sibling ) {
        for ( node=root->child; node; node=node->child ) {
            if ( ! node->child ) {
                /* This is the final leaf node on the trunk, select it */
                select_node(node, 1);
            }
        }
    }
}
