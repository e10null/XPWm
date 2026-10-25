/* Linux argv parsing, not cmd.exe/CreateProcess emulation. */
#include "run-command.h"
#include <gio/gdesktopappinfo.h>

gchar **run_command_parse(const char *command, GError **error)
{
	gchar **argv = NULL;
	if (!g_shell_parse_argv(command ? command : "", NULL, &argv, error))
		return NULL;
	if (!argv[0][0]) {
		g_set_error_literal(error, G_SHELL_ERROR, G_SHELL_ERROR_EMPTY_STRING,
				    "Enter a program name.");
		g_strfreev(argv);
		return NULL;
	}
	return argv;
}

/* Associations apply to an entire existing path (including spaces), one quoted
 * path, or a URI. Program arguments keep the original literal argv semantics. */
gchar *run_command_target_uri(const char *command)
{
	char *text = g_strdup(command ? command : "");
	g_strstrip(text);
	if (!*text) {
		g_free(text);
		return NULL;
	}
	char *scheme = g_uri_parse_scheme(text);
	if (scheme && !strpbrk(text, " \t\r\n")) {
		g_free(scheme);
		return text;
	}
	g_free(scheme);
	char *filename = g_canonicalize_filename(text, g_get_home_dir());
	if (g_file_test(filename, G_FILE_TEST_EXISTS) &&
	    (g_file_test(filename, G_FILE_TEST_IS_DIR) ||
	     !g_file_test(filename, G_FILE_TEST_IS_EXECUTABLE))) {
		char *uri = g_filename_to_uri(filename, NULL, NULL);
		g_free(filename);
		g_free(text);
		return uri;
	}
	g_free(filename);
	char **argv = NULL;
	char *uri = NULL;
	if (g_shell_parse_argv(text, NULL, &argv, NULL) && argv[0] && !argv[1]) {
		scheme = g_uri_parse_scheme(argv[0]);
		if (scheme)
			uri = g_strdup(argv[0]);
		else {
			filename = g_canonicalize_filename(argv[0], g_get_home_dir());
			if (g_file_test(filename, G_FILE_TEST_EXISTS) &&
			    (g_file_test(filename, G_FILE_TEST_IS_DIR) ||
			     !g_file_test(filename, G_FILE_TEST_IS_EXECUTABLE)))
				uri = g_filename_to_uri(filename, NULL, NULL);
			g_free(filename);
		}
		g_free(scheme);
	}
	g_strfreev(argv);
	g_free(text);
	return uri;
}

gboolean run_command_launch(const char *command, GError **error)
{
 /* A chosen application is launched through its desktop entry, preserving
  * Exec field codes, Terminal and D-Bus activation semantics. */
 char **desktop_argv=NULL;
 if(g_shell_parse_argv(command?command:"",NULL,&desktop_argv,NULL) && desktop_argv[0] && !desktop_argv[1]) {
  char *file=g_canonicalize_filename(desktop_argv[0],g_get_home_dir());
  if(g_str_has_suffix(file,".desktop") && g_file_test(file,G_FILE_TEST_IS_REGULAR)) {
   GDesktopAppInfo *app=g_desktop_app_info_new_from_filename(file);g_free(file);g_strfreev(desktop_argv);
   if(!app){g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_DATA,"Invalid desktop application entry");return FALSE;}
   gboolean result=g_app_info_launch(G_APP_INFO(app),NULL,NULL,error);g_object_unref(app);return result;
  }
  g_free(file);
 }
 g_strfreev(desktop_argv);
	char *uri = run_command_target_uri(command);
	if (uri) {
		gboolean launched = g_app_info_launch_default_for_uri(uri, NULL, error);
		g_free(uri);
		return launched;
	}
	gchar **argv = run_command_parse(command, error);
	gboolean result;
	if (!argv)
		return FALSE;
	/* No shell expansion, pipe handling or implicit elevation. Child is reaped
	 * by GLib; the desktop stays responsive and does not wait for it to exit. */
	result = g_spawn_async(g_get_home_dir(), argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, error);
	g_strfreev(argv);
	return result;
}
