
#include "patchset.h"

/* Take a set of patches and apply all the updates for existing components */
extern int auto_update(patchset *patchset);

/* Apply a set of patches to a product */
extern int perform_update(patch *patch);
