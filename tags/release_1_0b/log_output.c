
#include <stdio.h>
#include <stdarg.h>

#include "log_output.h"

static int log_level = LOG_NORMAL;

void set_logging(int level)
{
    log_level = level;
}

int get_logging(void)
{
    return(log_level);
}

void log(int level, const char *fmt, ...)
{
    if ( level >= log_level ) {
        va_list ap;

        va_start(ap, fmt);
        switch (level) {
            case LOG_ERROR:
                fprintf(stdout, "ERROR: ");
                break;
            case LOG_WARNING:
                fprintf(stdout, "WARNING: ");
                break;
            default:
                break;
        }
        vfprintf(stdout, fmt, ap);
        va_end(ap);
        fflush(stdout);
    }
}
