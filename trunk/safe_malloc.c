
#include <stdlib.h>
#include <string.h>

#include "safe_malloc.h"
#include "log_output.h"


void *safe_malloc(size_t size)
{
    void *mem;

    mem = malloc(size);
    if ( ! mem ) {
        log(LOG_ERROR, "Out of memory, allocating %d bytes\n", size);
        abort();
    }
    return(mem);
}

void *safe_realloc(void *mem, size_t size)
{
    mem = realloc(mem, size);
    if ( ! mem ) {
        log(LOG_ERROR, "Out of memory, allocating %d bytes\n", size);
        abort();
    }
    return(mem);
}

void safe_free(void *mem)
{
    if ( mem ) {
        free(mem);
    }
}

char *safe_strdup(const char *string)
{
    char *newstring;

    newstring = safe_malloc(strlen(string)+1);
    strcpy(newstring, string);
    return(newstring);
}
