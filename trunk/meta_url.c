
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

#include "text_parse.h"
#include "log_output.h"
#include "url_paths.h"
#include "urlset.h"
#include "get_url.h"
#include "load_products.h"
#include "meta_url.h"

static int download_progress(int status_level, const char *status,
                             float progress, int size, int total, void *udata)
{
    if ( status ) {
        log(status_level, "%s\n", status);
    }
    return(0);
}

void load_meta_url(const char *meta_url)
{
    char meta_file[PATH_MAX];
    struct text_fp *file;

    /* Download the meta file so we can parse it */
    compose_url(NULL, meta_url, meta_file, sizeof(meta_file));
    if ( get_url(meta_file, meta_file, sizeof(meta_file),
                 download_progress, NULL) != 0 ) {
        unlink(meta_file);
        return;
    }

    /* Open and parse the meta-file */
    file = text_open(meta_file);
    if ( file ) {
        char product_url[PATH_MAX];
        char key[1024], val[1024];

        while ( text_parsefield(file, key, sizeof(key), val, sizeof(val)) ) {
            compose_url(meta_url, val, product_url, sizeof(product_url));
            log(LOG_DEBUG,
                "Setting product url for '%s' to: %s\n", key, product_url);
            set_product_url(key, product_url);
        }
        text_close(file);
    }
    unlink(meta_file);
}
