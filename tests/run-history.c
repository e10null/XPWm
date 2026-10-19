#include <glib.h>
#include <glib/gstdio.h>
#include "../shell/run-history.h"
int main(void)
{
	GError *error = NULL;
	char *root = g_dir_make_tmp("xpwm-history-test-XXXXXX", &error);
	g_assert_no_error(error); g_setenv("XDG_CONFIG_HOME", root, TRUE);
	char **items = run_history_load(); g_assert_null(items[0]); g_strfreev(items);
	g_assert_true(run_history_record("  terminal --title='two words'  ", &error));
	g_assert_no_error(error);
	for (int i = 0; i < 30; i++) { char *v = g_strdup_printf("program-%d", i); g_assert_true(run_history_record(v, &error)); g_free(v); }
	g_assert_true(run_history_record("program-20", &error)); g_assert_no_error(error);
	items = run_history_load(); g_assert_cmpuint(g_strv_length(items), ==, 26);
	g_assert_cmpstr(items[0], ==, "program-20"); g_assert_cmpstr(items[1], ==, "program-29");
	for (guint i = 1; items[i]; i++) g_assert_cmpstr(items[i], !=, items[0]);
	g_strfreev(items);
	g_assert_true(run_history_record("quoted ';' '\\path'", &error));
	items = run_history_load(); g_assert_cmpstr(items[0], ==, "quoted ';' '\\path'"); g_strfreev(items);
	char *file = g_build_filename(root, "xpwm", "run-history.ini", NULL);
	char *dir = g_path_get_dirname(file); g_unlink(file); g_rmdir(dir); g_rmdir(root);
	g_free(dir); g_free(file); g_free(root);
	g_print("PASS: persistent Run history, most-recent ordering, deduplication, 26-item bound and literal text\n"); return 0;
}
