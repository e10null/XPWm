#include "../shell/taskbar-groups.h"
#include "../shell/taskbar-settings.h"
#include <glib/gstdio.h>
typedef struct { const char *id; } Window;
static const char *id(gpointer data) { return ((Window *)data)->id; }
int main(void)
{
 char *dir = g_dir_make_tmp("xpwm-groups-unit-XXXXXX", NULL); g_assert_nonnull(dir); g_setenv("XDG_CONFIG_HOME", dir, TRUE);
 Window items[] = {{"a"},{"b"},{"a"},{"c"},{"b"},{"a"},{NULL},{""},{"A"}};
 GList *windows = NULL;
 for (guint i = 0; i < G_N_ELEMENTS(items); i++) windows = g_list_append(windows, &items[i]);
 GPtrArray *groups = taskbar_groups_build(windows, id, TRUE, 900); g_assert_cmpuint(groups->len, ==, 9); g_ptr_array_unref(groups);
 groups = taskbar_groups_build(windows, id, FALSE, 0); g_assert_cmpuint(groups->len, ==, 9); g_ptr_array_unref(groups);
 groups = taskbar_groups_build(windows, id, TRUE, 700); g_assert_cmpuint(groups->len, ==, 7);
 GPtrArray *first = g_ptr_array_index(groups, 0); g_assert_cmpuint(first->len, ==, 3);
 g_assert_true(g_ptr_array_index(first, 0) == &items[0]); g_assert_true(g_ptr_array_index(first, 1) == &items[2]); g_assert_true(g_ptr_array_index(first, 2) == &items[5]);
 g_assert_true(g_ptr_array_index((GPtrArray *)g_ptr_array_index(groups, 1), 0) == &items[1]); g_ptr_array_unref(groups);
 groups = taskbar_groups_build(windows, id, TRUE, -1); g_assert_cmpuint(groups->len, ==, 6);
 g_assert_cmpuint(((GPtrArray *)g_ptr_array_index(groups, 1))->len, ==, 2); g_ptr_array_unref(groups); g_list_free(windows);
 groups = taskbar_groups_build(NULL, id, TRUE, 0); g_assert_cmpuint(groups->len, ==, 0); g_ptr_array_unref(groups);
 TaskbarSettings settings; taskbar_settings_load(&settings); g_assert_true(settings.group_similar);
 settings.group_similar = FALSE; GError *error = NULL; g_assert_true(taskbar_settings_save(&settings, &error)); g_assert_no_error(error);
 taskbar_settings_load(&settings); g_assert_false(settings.group_similar);
 char *folder = g_build_filename(dir, "xpwm", NULL), *path = g_build_filename(folder, "taskbar.ini", NULL);
 g_assert_true(g_file_set_contents(path, "[Taskbar]\nShowClock=false\nGroupSimilar=invalid\n", -1, NULL));
 taskbar_settings_load(&settings); g_assert_true(settings.group_similar); g_assert_false(settings.clock);
 g_remove(path); g_rmdir(folder); g_rmdir(dir); g_free(path); g_free(folder); g_free(dir);
 g_print("PASS: crowded grouping threshold, largest-first/stable order, separate unknown IDs, disabled/empty inputs and persisted/malformed preferences\n");
 return 0;
}
