/* Set the working directory for local relative path expansion */
extern void set_working_path(const char *cwd);

/* Compose a full URL from a base and a relative URL */
extern char *compose_url(const char *base, const char *url,
                         char *full, int maxlen);

