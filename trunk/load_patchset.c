
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "text_parse.h"
#include "patchset.h"
#include "log_output.h"
#include "load_patchset.h"

void print_patchset(patchset *patchset)
{
    version_node *node, *root, *trunk;
    int i;

    for ( root = patchset->root; root; root = root->sibling ) {
        for ( trunk = root; trunk; trunk = trunk->child ) {
            for ( node = trunk; node; node = node->sibling ) {
                if ( node->toggled ) {
                    printf("[X] ");
                } else {
                    printf("[ ] ");
                }
                for ( i=0; i<node->depth; ++i ) {
                    printf(" ");
                }
                printf("%s\n", node->description);
            }
        }
    }
}

patchset *load_patchset(patchset *patchset, const char *patchlist)
{
    struct text_fp *file;

    file = text_open(patchlist);
    if ( file ) {
        char parsed_name[1024];
        char *component = NULL;
        char *version = NULL;
        char *arch = NULL;
        char *applies = NULL;
        char *url = NULL;
        struct {
            const char *prefix;
            char **variable;
        } parse_table[] = {
            {   "Component", &component },
            {   "Version", &version },
            {   "Architecture", &arch },
            {   "Applies", &applies },
            {   "URL", &url },
        };
        int i;
        char key[1024], val[1024];

        /* Skip to the appropriate product section */
        parsed_name[0] = '\0';
        while ( strcasecmp(parsed_name, patchset->product_name) != 0 ) {
            if ( ! text_parsefield(file, key, sizeof(key), val, sizeof(val)) ) {
                break;
            }
            if ( strcasecmp(key, "product") == 0 ) {
                strcpy(parsed_name, val);
            }
        }

        /* Parse patches for this product */
        while ( strcasecmp(parsed_name, patchset->product_name) == 0 ) {
            if ( ! text_parsefield(file, key, sizeof(key), val, sizeof(val)) ) {
                break;
            }
            /* If there's a new product tag, check it above */
            if ( strcasecmp(key, "product") == 0 ) {
                strcpy(parsed_name, key);
                continue;
            }
            /* Look for known tags */
            for ( i=0; i<sizeof(parse_table)/sizeof(parse_table[0]); ++i ) {
                if ( strcasecmp(parse_table[i].prefix, key) == 0 ) {
                    if ( *parse_table[i].variable ) {
                        log(LOG_ERROR,
"Parse error in update list, %s appears twice in one update:\n",
                            parse_table[i].prefix);
                        log(LOG_ERROR, "First occurrence: %s\n",
                            *parse_table[i].variable);
                        log(LOG_ERROR, "Second occurrence: %s\n", val);
                        log(LOG_ERROR, "Parsed so far in this update:\n");
                        for ( i=0;
                              i<sizeof(parse_table)/sizeof(parse_table[0]);
                              ++i ) {
                            if ( *parse_table[i].variable ) {
                                log(LOG_ERROR, "%s: %s\n",
                                    parse_table[i].prefix,
                                    *parse_table[i].variable);
                            }
                        }
                        for ( i=0;
                              i<sizeof(parse_table)/sizeof(parse_table[0]);
                              ++i ) {
                            if ( ! *parse_table[i].variable ) {
                                log(LOG_ERROR, "Missing in parse: %s\n",
                                    parse_table[i].prefix);
                            }
                        }
                        goto done_parse;
                    } else {
                        *parse_table[i].variable = strdup(val);
                    }
                    break;
                }
            }
            if ( version && arch && applies && url ) {
                add_patch(patchset->product_name, component, version, arch, applies, url, patchset);
                for ( i=0; i<sizeof(parse_table)/sizeof(parse_table[0]); ++i ) {
                    if ( *parse_table[i].variable ) {
                        free(*parse_table[i].variable);
                        *parse_table[i].variable = NULL;
                    }
                }
            }
        }
done_parse:
        text_close(file);
    }

    /* Build a tree of patches and reduce it to the most efficient set */
    calculate_paths(patchset);
    autoselect_patches(patchset);
#ifdef DEBUG
    print_patchset(patchset);
#endif
    return patchset;
}

char *get_product_description(product_t *product, char *description, int maxlen)
{
#if 0 /* Not really necessary */
    product_component_t *component;
#endif

    strncpy(description, loki_getinfo_product(product)->description, maxlen-2);
    description[maxlen-2] = '\0';
#if 0 /* Not really necessary */
    component = loki_getdefault_component(product);
    if ( component ) {
        strcat(description, " ");
        maxlen -= strlen(description);
        strncat(description, loki_getversion_component(component), maxlen-1);
    }
#endif
    return(description);
}
