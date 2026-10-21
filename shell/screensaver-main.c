#include "screensaver-view.h"
#include "../shared/luna.h"
#include <gtk-layer-shell.h>

static gint64 shown;
static gboolean dismiss(GtkWidget *widget,GdkEvent *event,gpointer data)
{
 (void)widget;(void)data;
 if(event->type==GDK_MOTION_NOTIFY && g_get_monotonic_time()-shown<300000) return TRUE;
 gtk_main_quit();return TRUE;
}
int main(int argc,char **argv)
{
 int mode=-1;char *value=NULL,*pictures=NULL;GError *error=NULL;
 GOptionEntry options[]={
  {"mode",0,0,G_OPTION_ARG_INT,&mode,"Screen saver number", "N"},
  {"text",0,0,G_OPTION_ARG_STRING,&value,"Screen saver text","TEXT"},
  {"pictures",0,0,G_OPTION_ARG_FILENAME,&pictures,"Slideshow folder","DIRECTORY"},
  {NULL,0,0,0,NULL,NULL,NULL}};
 GOptionContext *context=g_option_context_new("— native XP-style screen savers");
 g_option_context_add_main_entries(context,options,NULL);g_option_context_add_group(context,gtk_get_option_group(TRUE));
 if(!g_option_context_parse(context,&argc,&argv,&error)){g_printerr("%s\n",error->message);g_error_free(error);g_option_context_free(context);return 1;}
 g_option_context_free(context);
 if(!gtk_layer_is_supported()){g_printerr("XPWm screen savers require Wayland layer-shell.\n");return 1;}
 XpSaverConfig config;xp_saver_config_load(&config);
 if(mode>=0 && mode<XP_SAVER_COUNT) config.mode=mode;
 if(value){g_free(config.text);config.text=value;}
 if(pictures){g_free(config.pictures);config.pictures=pictures;}
 if(config.mode==XP_SAVER_NONE){xp_saver_config_clear(&config);return 0;}
 luna_init();shown=g_get_monotonic_time();GPtrArray *windows=g_ptr_array_new();
 GdkDisplay *display=gdk_display_get_default();
 for(int i=0;i<gdk_display_get_n_monitors(display);i++) {
  GtkWidget *window=gtk_window_new(GTK_WINDOW_TOPLEVEL);gtk_window_set_title(GTK_WINDOW(window),xp_saver_names[config.mode]);
  gtk_layer_init_for_window(GTK_WINDOW(window));gtk_layer_set_namespace(GTK_WINDOW(window),"xpwm-screensaver");
  gtk_layer_set_monitor(GTK_WINDOW(window),gdk_display_get_monitor(display,i));
  gtk_layer_set_layer(GTK_WINDOW(window),GTK_LAYER_SHELL_LAYER_OVERLAY);gtk_layer_set_exclusive_zone(GTK_WINDOW(window),-1);
  gtk_layer_set_keyboard_mode(GTK_WINDOW(window),GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
  for(int edge=0;edge<GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER;edge++)gtk_layer_set_anchor(GTK_WINDOW(window),edge,TRUE);
  gtk_container_add(GTK_CONTAINER(window),xp_saver_view_new(&config));
  gtk_widget_add_events(window,GDK_KEY_PRESS_MASK|GDK_BUTTON_PRESS_MASK|GDK_POINTER_MOTION_MASK|GDK_SCROLL_MASK);
  const char *events[]={"key-press-event","button-press-event","motion-notify-event","scroll-event"};
  for(guint j=0;j<G_N_ELEMENTS(events);j++)g_signal_connect(window,events[j],G_CALLBACK(dismiss),NULL);
  gtk_widget_show_all(window);
  GdkWindow *gdk=gtk_widget_get_window(window);
  if(gdk){GdkCursor *cursor=gdk_cursor_new_for_display(display,GDK_BLANK_CURSOR);gdk_window_set_cursor(gdk,cursor);g_object_unref(cursor);}
  g_ptr_array_add(windows,window);
 }
 if(windows->len)gtk_main();
 for(guint i=0;i<windows->len;i++)gtk_widget_destroy(g_ptr_array_index(windows,i));
 g_ptr_array_unref(windows);xp_saver_config_clear(&config);luna_finish();return 0;
}
