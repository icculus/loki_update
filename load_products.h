
extern void load_product_list(void);

extern const char *get_first_product(void);
extern const char *get_next_product(void);

extern int is_valid_product(const char *product);

extern void set_override_url(const char *update_url);
extern void set_product_root(const char *product, const char *root);
extern void set_product_url(const char *product, const char *url);

extern const char *get_product_version(const char *product);
extern const char *get_product_description(const char *product);
extern const char *get_product_root(const char *product);
extern const char *get_product_url(const char *product);
extern const char *get_default_component(const char *product);

extern void free_product_list(void);
