
#ifndef _LOG_OUTPUT_H
#define _LOG_OUTPUT_H

enum {
    LOG_DEBUG = 0,
    LOG_VERBOSE,
    LOG_STATUS,
    LOG_NORMAL,
    LOG_WARNING,
    LOG_ERROR,
    LOG_NONE
};

extern void set_logging(int level);
extern int get_logging(void);
extern void log(int level, const char *fmt, ...);

#endif /* _LOG_OUTPUT_H */
