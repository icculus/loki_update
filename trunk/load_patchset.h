
#include "patchset.h"

extern char *get_product_description(product_t *product,
                                     char *description, int maxlen);

extern patchset *load_patchset(patchset *patchset, const char *patchlist);
extern void print_patchset(patchset *patchset);
