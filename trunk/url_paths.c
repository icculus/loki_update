
#include <string.h>
#include <limits.h>

#include "url_paths.h"

static char working_path[PATH_MAX];

void set_working_path(const char *cwd)
{
    strncpy(working_path, cwd, sizeof(working_path));
}

/* Compose a full URL from a base and a relative URL */
char *compose_url(const char *base, const char *url, char *full, int maxlen)
{
    char *bufp;

    bufp = strstr(url, "://");
    if ( base && ! bufp && (*url != '/') ) {
        bufp = strstr(base, "://");
        if ( working_path[0] && ! bufp && (*base != '/') ) {
            strncpy(full, working_path, maxlen-1);
            strcat(full, "/");
            strncat(full, base, maxlen-strlen(full));
        } else {
            strncpy(full, base, maxlen);
        }
        bufp = strrchr(full, '/');
        if ( bufp++ ) {
            *bufp = '\0';
        }
        strncat(full, url, maxlen-strlen(full));
    } else {
        strncpy(full, url, maxlen);
    }
    return(full);
}

