#include <glib.h>
#include <glib/gstdio.h>
#include "../shell/run-command.h"

int main(int argc, char **argv)
{
	GError *error = NULL;
	char **parsed, *directory, *marker, *quoted, *command, *contents = NULL;
	if (argc == 4 && !g_strcmp0(argv[1], "--child")) {
		return g_file_set_contents(argv[2], argv[3], -1, NULL) ? 0 : 1;
	}
	parsed = run_command_parse("program 'two words' \"\" '$HOME' 'a;b'", &error);
	g_assert_no_error(error);
	g_assert_cmpuint(g_strv_length(parsed), ==, 5);
	g_assert_cmpstr(parsed[1], ==, "two words");
	g_assert_cmpstr(parsed[2], ==, "");
	g_assert_cmpstr(parsed[3], ==, "$HOME");
	g_assert_cmpstr(parsed[4], ==, "a;b");
	g_strfreev(parsed);
	const char *invalid[] = {"", "   ", "'' argument", "program 'unterminated"};
	for (unsigned i = 0; i < G_N_ELEMENTS(invalid); i++) {
		g_assert_null(run_command_parse(invalid[i], &error));
		g_assert_nonnull(error); g_clear_error(&error);
	}
	g_assert_false(run_command_launch("/nonexistent/xpwm-test-program", &error));
	g_assert_nonnull(error); g_clear_error(&error);
	directory = g_dir_make_tmp("xpwm-run-test-XXXXXX", &error);
	g_assert_no_error(error);
	marker = g_build_filename(directory, "arguments.txt", NULL);
	quoted = g_shell_quote(marker);
	command = g_strdup_printf("/proc/self/exe --child %s '$HOME;literal'", quoted);
	g_assert_true(run_command_launch(command, &error));
	g_assert_no_error(error);
	for (int i = 0; i < 100 && !contents; i++) {
		g_usleep(10000);
		g_file_get_contents(marker, &contents, NULL, NULL);
	}
	g_assert_cmpstr(contents, ==, "$HOME;literal");

 g_assert_null(run_command_target_uri(""));
 g_assert_null(run_command_target_uri("  "));
 g_assert_null(run_command_target_uri("program --arg"));
 char *url = run_command_target_uri("https://example.invalid/path?q=a%20b");
 g_assert_cmpstr(url, ==, "https://example.invalid/path?q=a%20b"); g_free(url);
 char *document = g_build_filename(directory, "two words.txt", NULL);
 g_assert_true(g_file_set_contents(document, "test", -1, NULL));
 char *expected_uri = g_filename_to_uri(document, NULL, NULL);
 url = run_command_target_uri(document); g_assert_cmpstr(url, ==, expected_uri); g_free(url);
 char *quoted_document = g_shell_quote(document);
 url = run_command_target_uri(quoted_document); g_assert_cmpstr(url, ==, expected_uri); g_free(url);
 url = run_command_target_uri(directory); g_assert_nonnull(url); g_free(url);
 g_free(quoted_document); g_free(expected_uri); g_unlink(document); g_free(document);
	g_unlink(marker); g_rmdir(directory);
	g_free(contents); g_free(command); g_free(quoted); g_free(marker); g_free(directory);
	g_print("PASS: Run quoting, empty/malformed commands, missing executable, literal-argument launch and URI/document/folder classification\n");
	return 0;
}
