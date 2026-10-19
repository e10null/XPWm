#ifndef XPWM_TASKBAR_GROUPS_H
#define XPWM_TASKBAR_GROUPS_H
#include <glib.h>
/* Groups borrow their window pointers; the returned array owns its arrays. */
GPtrArray *taskbar_groups_build(GList *windows, const char *(*app_id)(gpointer), gboolean enabled, int available);
#endif
