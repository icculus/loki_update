
#include <stdlib.h>

extern void *safe_malloc(size_t size);
extern void *safe_realloc(void *mem, size_t size);
extern void safe_free(void *mem);

extern char *safe_strdup(const char *string);
