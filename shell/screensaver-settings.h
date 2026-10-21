#ifndef XPWM_SCREENSAVER_SETTINGS_H
#define XPWM_SCREENSAVER_SETTINGS_H
#include <gtk/gtk.h>
GtkWidget *xp_saver_settings_new(void (*changed)(gpointer),gpointer data);
gboolean xp_saver_settings_dirty(GtkWidget *page);
gboolean xp_saver_settings_apply(GtkWidget *page,GError **error);
#endif
