
extern int perform_update(const char *update_file,
    int (*update)(float percentage, int size, int total, void *udata),
                                                void *udata);
