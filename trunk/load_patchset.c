
#include <stdlib.h>
#include <string.h>

#include "text_parse.h"
#include "patchset.h"
#include "log_output.h"
#include "load_patchset.h"

patchset *load_patchset(patchset *patchset, const char *patchlist)
{
    struct text_fp *file;
    int index;
    patch *patch;
    patch_path *path;

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
                    *parse_table[i].variable = strdup(val);
                    break;
                }
            }
            if ( version && arch && applies && url ) {
                log(LOG_DEBUG,
                    "Found patch: %s %s for %s applies to %s, at %s\n",
                     patchset->product_name, version, arch, applies, url);
                add_patch(patchset->product_name, component, version, arch, applies, url, patchset);
                for ( i=0; i<sizeof(parse_table)/sizeof(parse_table[0]); ++i ) {
                    if ( *parse_table[i].variable ) {
                        free(*parse_table[i].variable);
                        *parse_table[i].variable = NULL;
                    }
                }
            }
        }
        text_close(file);
    }

    /* Build a tree of patches and reduce it to the most efficient set */
    make_tree(patchset);
    log(LOG_DEBUG, "Patch set for %s\n",
        loki_getinfo_product(patchset->product)->name);
    index = 0;
    for ( path=patchset->paths; path; path=path->next ) {
        log(LOG_DEBUG, "Patch path %d:\n", ++index);
        for ( patch=path->patches; patch; patch=patch->next ) {
            log(LOG_DEBUG, "\tPatch: %s at %s\n", patch->version, patch->url);
        }
    }
    collapse_tree(patchset);
    log(LOG_DEBUG, "Collapsed patch set:\n");
    index = 0;
    for ( path=patchset->paths; path; path=path->next ) {
        log(LOG_DEBUG, "Patch path %d:\n", ++index);
        for ( patch=path->patches; patch; patch=patch->next ) {
            log(LOG_DEBUG, "\tPatch: %s at %s\n", patch->version, patch->url);
        }
    }
    return patchset;
}
