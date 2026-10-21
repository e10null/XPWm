#ifndef XPWM_GTK_FONT_SETTINGS_H
#define XPWM_GTK_FONT_SETTINGS_H
#include <gtk/gtk.h>
#include "../shared/font-settings.h"
static gboolean xp_font_settings_refresh(gpointer data) {
 GtkSettings *settings=data;int current=0,wanted=xp_font_dpi()*1024;
 g_object_get(settings,"gtk-xft-dpi",&current,NULL);
 if(current!=wanted)g_object_set(settings,"gtk-xft-dpi",wanted,NULL);
 return G_SOURCE_CONTINUE;
}
static void xp_font_settings_watch(GtkSettings *settings) {
 if(!settings || g_object_get_data(G_OBJECT(settings),"xp-font-settings-watch"))return;
 g_object_set_data(G_OBJECT(settings),"xp-font-settings-watch",GINT_TO_POINTER(1));
 xp_font_settings_refresh(settings);g_timeout_add_full(G_PRIORITY_DEFAULT,500,xp_font_settings_refresh,g_object_ref(settings),g_object_unref);
}
#endif
