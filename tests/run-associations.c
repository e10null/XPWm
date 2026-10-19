/* All associations and launchers are private; never opens a host application. */
#include <gio/gio.h>
#include <glib/gstdio.h>
#include "../shell/run-command.h"
static void remove_tree(const char *path)
{
	GDir *dir = g_dir_open(path, 0, NULL);
	const char *name;
	while (dir && (name = g_dir_read_name(dir))) {
		char *child = g_build_filename(path, name, NULL);
		if (g_file_test(child, G_FILE_TEST_IS_DIR) && !g_file_test(child, G_FILE_TEST_IS_SYMLINK)) remove_tree(child);
		else g_unlink(child);
		g_free(child);
	}
	if (dir) g_dir_close(dir);
	g_rmdir(path);
}
static char *wait_marker(const char *path)
{
	char *text = NULL;
	for (int i = 0; i < 200 && !text; i++) { g_usleep(10000); g_file_get_contents(path, &text, NULL, NULL); }
	g_assert_nonnull(text); g_unlink(path); return text;
}
int main(int argc, char **argv)
{
	if (argc == 4 && !g_strcmp0(argv[1], "--open-child"))
		return g_file_set_contents(argv[2], argv[3], -1, NULL) ? 0 : 1;
	GError *error = NULL;
	char *root = g_dir_make_tmp("xpwm-associations-XXXXXX", &error); g_assert_no_error(error);
	char *config = g_build_filename(root, "config", NULL), *data = g_build_filename(root, "data", NULL);
	g_setenv("XDG_CONFIG_HOME", config, TRUE); g_setenv("XDG_DATA_HOME", data, TRUE); g_setenv("GIO_USE_VFS", "local", TRUE);
	char *marker = g_build_filename(root, "opened", NULL), *exe = g_file_read_link("/proc/self/exe", NULL);
	char *program = g_shell_quote(exe), *target = g_shell_quote(marker);
	char *command = g_strdup_printf("%s --open-child %s", program, target);
	GAppInfo *app = g_app_info_create_from_commandline(command, "XPWm Association Fixture", G_APP_INFO_CREATE_SUPPORTS_URIS, &error);
	g_assert_no_error(error); g_assert_nonnull(app);
	const char *types[] = {"x-scheme-handler/xpwm-test", "text/plain", "inode/directory"};
	for (guint i = 0; i < G_N_ELEMENTS(types); i++) {
		g_assert_true(g_app_info_set_as_default_for_type(app, types[i], &error)); g_assert_no_error(error);
	}
	g_assert_true(run_command_launch("xpwm-test://example/path", &error)); g_assert_no_error(error);
	char *opened = wait_marker(marker); g_assert_cmpstr(opened, ==, "xpwm-test://example/path"); g_free(opened);
	char *document = g_build_filename(root, "two words.txt", NULL);
	g_assert_true(g_file_set_contents(document, "A plain text fixture.\n", -1, NULL));
	g_assert_true(run_command_launch(document, &error)); g_assert_no_error(error);
	opened = wait_marker(marker); char *uri = g_filename_to_uri(document, NULL, NULL);
	g_assert_true(!g_strcmp0(opened, uri) || !g_strcmp0(opened, document)); g_free(opened); g_free(uri);
	g_assert_true(run_command_launch(root, &error)); g_assert_no_error(error);
	opened = wait_marker(marker); uri = g_filename_to_uri(root, NULL, NULL);
	g_assert_true(!g_strcmp0(opened, uri) || !g_strcmp0(opened, root)); g_free(opened); g_free(uri);
	g_app_info_delete(app); g_object_unref(app); remove_tree(root);
	g_free(document); g_free(command); g_free(program); g_free(target); g_free(exe); g_free(marker); g_free(config); g_free(data); g_free(root);
	g_print("PASS: Run actually opens custom URLs, spaced document paths and folders through private associations\n"); return 0;
}
