
/* This is a simple function to retrieve a URL and save it to the
   update directory.
*/

extern int get_url(const char *url, char *file, int maxpath,
    int (*update)(float percentage, int size, int total, void *udata),
                                                void *udata);
