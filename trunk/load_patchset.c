
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "text_parse.h"
#include "log_output.h"
#include "patchset.h"
#include "url_paths.h"
#include "load_products.h"
#include "load_patchset.h"

void print_patchset(patchset *patchset)
{
    version_node *node, *root, *trunk;

    for ( root = patchset->root; root; root = root->sibling ) {
        for ( trunk = root; trunk; trunk = trunk->child ) {
            for ( node = trunk; node; node = node->sibling ) {
                if ( node->toggled ) {
                    printf("[X] ");
                } else {
                    printf("[ ] ");
                }
                printf("%s\n", node->description);
            }
        }
    }
}

/* Variables used to store patch information during parsing */
static char parsed_name[1024];
static char *component = NULL;
static char *version = NULL;
static char *arch = NULL;
static char *applies = NULL;
static char *note = NULL;
static urlset *patch_urls = NULL;
struct {
    const char *prefix;
    int optional;
    int expandable;
    char **variable;
} parse_table[] = {
    {   "Component", 1, 0, &component },
    {   "Version", 0, 0, &version },
    {   "Architecture", 0, 1, &arch },
    {   "Applies", 0, 1, &applies },
    {   "Note", 1, 0, &note }
};

/* Verify all the parameters and add the current patch to the patchset */
static int check_and_add_patch(patchset *patchset)
{
    int i;
    int status;

    /* If there are no tags at all, that's fine, successful end of parse */
    status = 0;
    for ( i=0; i<sizeof(parse_table)/sizeof(parse_table[0]); ++i ) {
        if ( *parse_table[i].variable ) {
            ++status;
        }
    }
    if ( status == 0 ) {
        return(0);
    }

    /* Check for missing tags */
    status = 0;
    for ( i=0; i<sizeof(parse_table)/sizeof(parse_table[0]); ++i ) {
        if ( ! *parse_table[i].variable && ! parse_table[i].optional ) {
            log(LOG_ERROR, "Missing in parse: %s\n", parse_table[i].prefix);
            status = -1;
        }
    }
    if ( ! patch_urls->urls ) {
        log(LOG_ERROR, "No update URL defined for this update\n");
        status = -1;
    }
    if ( status != 0 ) {
        log(LOG_ERROR, "Parsed so far in this update for %s:\n", parsed_name);
        for ( i=0; i<sizeof(parse_table)/sizeof(parse_table[0]); ++i ) {
            if ( *parse_table[i].variable ) {
                log(LOG_ERROR, "%s: %s\n",
                    parse_table[i].prefix, *parse_table[i].variable);
            }
        }
    }

    /* Add the patch to our patchset */
    if ( status == 0 ) {
        add_patch(patchset->product_name, component, version,
                  arch, applies, note, patch_urls, patchset);
    } else {
        free_urlset(patch_urls);
    }

    /* Clean up for the next patch */
    for ( i=0; i<sizeof(parse_table)/sizeof(parse_table[0]); ++i ) {
        if ( *parse_table[i].variable ) {
            free(*parse_table[i].variable);
            *parse_table[i].variable = NULL;
        }
    }
    patch_urls = create_urlset();

    return(status);
}

patchset *load_patchset(patchset *patchset, const char *patchlist)
{
    struct text_fp *file;

    file = text_open(patchlist);
    if ( file ) {
        int i;
        char key[1024], val[1024];
        char url[2048];

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
        patch_urls = create_urlset();
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
                        if ( parse_table[i].expandable ) {
                            char tmp[1024];
                            snprintf(tmp, sizeof(tmp), "%s, %s",
                                     *parse_table[i].variable, val);
                            free(*parse_table[i].variable);
                            *parse_table[i].variable = strdup(tmp);
                        } else {
                            if ( check_and_add_patch(patchset) < 0 ) {
                                /* Error, messages already output */
                                goto done_parse;
                            }
                            *parse_table[i].variable = strdup(val);
                        }
                    } else {
                        *parse_table[i].variable = strdup(val);
                    }
                    break;
                }
            }
            if ( (strncasecmp("URL", key, 3) == 0) &&
                 (!key[3] || isdigit(key[3])) ) {
                compose_url(get_product_url(parsed_name),
                            val, url, sizeof(url));
                add_url(patch_urls, url, atoi(&key[3]));
            }
        }
        check_and_add_patch(patchset);
done_parse:
        free_urlset(patch_urls);
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
