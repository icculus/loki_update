/* Set/Get the original working directory for local relative path expansion */
extern void set_working_path(const char *cwd);
extern const char *get_working_path(void);

/* Compose a full URL from a base and a relative URL */
extern char *compose_url(const char *base, const char *url,
                         char *full, int maxlen);

