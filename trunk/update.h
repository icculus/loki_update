
extern int perform_update(const char *update_file, const char *install_path,
    int (*update)(float percentage, int size, int total, void *udata),
                                                void *udata);
