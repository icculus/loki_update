
/* A set of URLs, an opaque pointer */
typedef struct url_bucket url_bucket_list;
typedef struct urlset {
    url_bucket_list *urls;
    url_bucket_list *current;
} urlset;


/* Create a set of update URLs */
extern urlset *create_urlset(void);

/* Add a URL to a set of update URLs */
extern void add_url(urlset *urlset, const char *url, int order);

/* Get the next URL to be tried for an update */
extern const char *get_next_url(urlset *urlset);

/* Free a set of update URLs */
extern void free_urlset(urlset *urlset);
