/* Linux argv parsing, not cmd.exe/CreateProcess emulation. */
#include "run-command.h"

gchar **run_command_parse(const char *command, GError **error)
{
	gchar **argv = NULL;
	if (!g_shell_parse_argv(command ? command : "", NULL, &argv, error)) return NULL;
	if (!argv[0][0]) {
		g_set_error_literal(error, G_SHELL_ERROR, G_SHELL_ERROR_EMPTY_STRING,
			"Enter a program name.");
		g_strfreev(argv);
		return NULL;
	}
	return argv;
}

gboolean run_command_launch(const char *command, GError **error)
{
	gchar **argv = run_command_parse(command, error);
	gboolean result;
	if (!argv) return FALSE;
	/* No shell expansion, pipe handling or implicit elevation. Child is reaped
	 * by GLib; the desktop stays responsive and does not wait for it to exit. */
	result = g_spawn_async(g_get_home_dir(), argv, NULL, G_SPAWN_SEARCH_PATH,
		NULL, NULL, NULL, error);
	g_strfreev(argv);
	return result;
}
