
enum {
    LOG_DEBUG,
    LOG_VERBOSE,
    LOG_NORMAL,
    LOG_WARNING,
    LOG_ERROR
};

extern void set_logging(int level);
extern void log(int level, const char *fmt, ...);
extern void log_warning(const char *fmt, ...);
