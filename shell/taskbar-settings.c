#include "taskbar-settings.h"
#include <errno.h>
static const char *keys[] = {"ShowClock", "ShowQuickLaunch", "AutoHide", "AlwaysOnTop", "GroupSimilar"};
static char *settings_path(void) { return g_build_filename(g_get_user_config_dir(), "xpwm", "taskbar.ini", NULL); }
void taskbar_settings_load(TaskbarSettings *s)
{
 gboolean *values[] = {&s->clock, &s->quick_launch, &s->auto_hide, &s->on_top, &s->group_similar};
 *s = (TaskbarSettings){TRUE, TRUE, FALSE, TRUE, TRUE};
 char *path = settings_path(); GKeyFile *ini = g_key_file_new();
 if (g_key_file_load_from_file(ini, path, G_KEY_FILE_NONE, NULL))
  for (guint i = 0; i < G_N_ELEMENTS(keys); i++) {
   GError *error = NULL;
   gboolean value = g_key_file_get_boolean(ini, "Taskbar", keys[i], &error);
   if (!error) *values[i] = value;
   g_clear_error(&error);
  }
 g_free(path); g_key_file_unref(ini);
}
gboolean taskbar_settings_save(const TaskbarSettings *s, GError **error)
{
 const gboolean values[] = {s->clock, s->quick_launch, s->auto_hide, s->on_top, s->group_similar};
 char *path = settings_path(), *dir = g_path_get_dirname(path);
 GKeyFile *ini = g_key_file_new();
 g_key_file_load_from_file(ini, path, G_KEY_FILE_KEEP_COMMENTS, NULL);
 for (guint i = 0; i < G_N_ELEMENTS(keys); i++) g_key_file_set_boolean(ini, "Taskbar", keys[i], values[i]);
 gboolean ok = g_mkdir_with_parents(dir, 0700) == 0;
 if (!ok) g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno), "Unable to create taskbar settings directory: %s", g_strerror(errno));
 char *text = g_key_file_to_data(ini, NULL, NULL);
 if (ok) ok = g_file_set_contents(path, text, -1, error);
 g_free(text); g_free(dir); g_free(path); g_key_file_unref(ini); return ok;
}
