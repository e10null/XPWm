#ifndef XPWM_DISPLAY_SETTINGS_H
#define XPWM_DISPLAY_SETTINGS_H
#include <gtk/gtk.h>
GtkWidget *xp_display_settings_new(GtkWindow *parent, void (*changed)(gpointer), gpointer data);
gboolean xp_display_settings_dirty(GtkWidget *page);
gboolean xp_display_settings_busy(GtkWidget *page);
void xp_display_settings_apply(GtkWidget *page, void (*done)(gboolean, gpointer), gpointer data);
#endif
