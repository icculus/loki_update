
/* The user interface plugin API */

#define PRODUCT     "Loki_Update"

typedef struct {
    int (*detect)(void);
    int (*init)(int argc, char *argv[]);
    int (*auto_update)(const char *product);
    int (*perform_updates)(const char *product);
    void (*cleanup)(void);
} update_UI;

extern update_UI gtk_ui;
