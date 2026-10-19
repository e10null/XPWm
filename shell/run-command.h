#ifndef XPWM_RUN_COMMAND_H
#define XPWM_RUN_COMMAND_H
#include <gio/gio.h>
gchar **run_command_parse(const char *command, GError **error);
gchar *run_command_target_uri(const char *command);
gboolean run_command_launch(const char *command, GError **error);
#endif
