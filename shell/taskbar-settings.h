#ifndef XPWM_TASKBAR_SETTINGS_H
#define XPWM_TASKBAR_SETTINGS_H
#include <glib.h>
typedef struct { gboolean clock, quick_launch, auto_hide, on_top, group_similar, focus_follows_mouse; } TaskbarSettings;
void taskbar_settings_load(TaskbarSettings *settings);
gboolean taskbar_settings_save(const TaskbarSettings *settings, GError **error);
#endif
