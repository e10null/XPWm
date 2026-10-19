#ifndef XPWM_RUN_HISTORY_H
#define XPWM_RUN_HISTORY_H
#include <glib.h>
gchar **run_history_load(void);
gboolean run_history_record(const char *command, GError **error);
#endif
