
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "text_parse.h"
#include "patchset.h"
#include "get_url.h"
#include "log_output.h"
#include "load_patchset.h"

patchset *load_patchset(const char *product_name)
{
    patchset *patchset;
    char update_url[PATH_MAX];
    struct text_fp *file;
    int index;
    patch *patch;
    patch_path *path;

    /* Create the patchset that we'll use */
    patchset = create_patchset(product_name);
    if ( ! patchset ) {
        return(NULL);
    }

    /* Retrieve the update URL and parse it for patches */
    strcpy(update_url, loki_getinfo_product(patchset->product)->url);
    if ( get_url(update_url, update_url, sizeof(update_url)) != 0 ) {
        log(LOG_ERROR, "Unable to retrieve URL:\n%s\n", update_url);
        free_patchset(patchset);
        return(NULL);
    }
    file = text_open(update_url);
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
        while ( strcasecmp(parsed_name, product_name) != 0 ) {
            if ( ! text_parsefield(file, key, sizeof(key), val, sizeof(val)) ) {
                break;
            }
            if ( strcasecmp(key, "product") == 0 ) {
                strcpy(parsed_name, val);
            }
        }

        /* Parse patches for this product */
        while ( strcasecmp(parsed_name, product_name) == 0 ) {
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
                     product_name, version, arch, applies, url);
                add_patch(product_name, component, version, arch, applies, url, patchset);
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
    unlink(update_url);

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
