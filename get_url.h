
/* This is a simple function to retrieve a URL and save it to the
   update directory.
*/

#include "update.h"

extern int get_url(const char *url, char *file, int maxpath,
                   update_callback update, void *udata);
