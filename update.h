
/* Header file for the update function and the download update callback */

#ifndef _UPDATE_H
#define _UPDATE_H

#include "log_output.h"

typedef int (*update_callback)(int status_level, const char *status,
                               float percentage, int size, int total,
                               float rate, void *udata);

extern void update_message(int level, const char *message,
                           update_callback update, void *udata);

extern int perform_update(const char *update_file, const char *install_path,
                          update_callback update, void *udata);

#endif /* _UPDATE_H */
