#ifndef XPWM_RUN_COMMAND_H
#define XPWM_RUN_COMMAND_H
#include <glib.h>
gchar **run_command_parse(const char *command, GError **error);
gboolean run_command_launch(const char *command, GError **error);
#endif
