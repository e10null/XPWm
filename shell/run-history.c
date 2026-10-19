/* Per-user Run MRU, shared by the standalone launcher and taskbar dialog. */
#include "run-history.h"
#include <string.h>
#define HISTORY_LIMIT 26
static char *path(void) { return g_build_filename(g_get_user_config_dir(), "xpwm", "run-history.ini", NULL); }
gchar **run_history_load(void)
{
	char *filename = path();
	GKeyFile *ini = g_key_file_new();
	g_key_file_load_from_file(ini, filename, G_KEY_FILE_NONE, NULL);
	gsize count = 0;
	char **values = g_key_file_get_string_list(ini, "Run", "commands", &count, NULL);
	GPtrArray *result = g_ptr_array_new_with_free_func(g_free);
	for (gsize i = 0; values && i < count && result->len < HISTORY_LIMIT; i++) {
		if (!*values[i] || !g_utf8_validate(values[i], -1, NULL))
			continue;
		gboolean duplicate = FALSE;
		for (guint j = 0; j < result->len; j++)
			if (!strcmp(values[i], g_ptr_array_index(result, j)))
				duplicate = TRUE;
		if (!duplicate)
			g_ptr_array_add(result, g_strdup(values[i]));
	}
	g_ptr_array_add(result, NULL);
	g_strfreev(values);
	g_key_file_unref(ini);
	g_free(filename);
	return (char **)g_ptr_array_free(result, FALSE);
}
gboolean run_history_record(const char *command, GError **error)
{
	char *value = g_strdup(command ? command : "");
	g_strstrip(value);
	if (!*value) {
		g_free(value);
		return TRUE;
	}
	char **old = run_history_load();
	GPtrArray *list = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(list, value);
	for (guint i = 0; old[i] && list->len < HISTORY_LIMIT; i++)
		if (strcmp(old[i], value))
			g_ptr_array_add(list, g_strdup(old[i]));
	GKeyFile *ini = g_key_file_new();
	g_key_file_set_string_list(ini, "Run", "commands", (const char *const *)list->pdata, list->len);
	char *filename = path(), *directory = g_path_get_dirname(filename);
	gboolean result = g_mkdir_with_parents(directory, 0700) == 0;
	if (!result)
		g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_ACCES, "Unable to save Run history.");
	char *text = g_key_file_to_data(ini, NULL, NULL);
	if (result)
		result = g_file_set_contents(filename, text, -1, error);
	g_free(text);
	g_free(directory);
	g_free(filename);
	g_key_file_unref(ini);
	g_ptr_array_unref(list);
	g_strfreev(old);
	return result;
}
