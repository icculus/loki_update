
enum {
    LOG_DEBUG,
    LOG_VERBOSE,
    LOG_NORMAL,
    LOG_WARNING,
    LOG_ERROR
};

extern void set_logging(int level);
extern int get_logging(void);
extern void log(int level, const char *fmt, ...);
