#ifndef XPWM_GTK_FONT_SETTINGS_H
#define XPWM_GTK_FONT_SETTINGS_H
#include <gtk/gtk.h>
#include "../shared/font-settings.h"
static gboolean xp_button_label_draw(GtkWidget *label,cairo_t *cr,gpointer data) {
 PangoLayout *layout=gtk_label_get_layout(GTK_LABEL(label));
 PangoRectangle ink;
 pango_layout_get_pixel_extents(layout,&ink,NULL);
 /* Center visible glyphs, rather than the font's ascent/descent line box.
  * Tahoma leaves extra descent space even in labels with no descenders. */
 double x=(gtk_widget_get_allocated_width(label)-ink.width)/2.-ink.x;
 double y=(gtk_widget_get_allocated_height(label)-ink.height)/2.-ink.y;
 gtk_render_layout(gtk_widget_get_style_context(label),cr,x,y,layout);
 return TRUE;
}
static gboolean xp_button_center(GSignalInvocationHint *hint,guint count,const GValue *values,gpointer data) {
 GtkWidget *widget=g_value_get_object(&values[0]);
 if(GTK_IS_BUTTON(widget) && !GTK_IS_CHECK_BUTTON(widget) &&
    !gtk_widget_get_ancestor(widget,GTK_TYPE_COMBO_BOX) &&
    !gtk_widget_get_ancestor(widget,GTK_TYPE_TREE_VIEW)) {
  gtk_button_set_alignment(GTK_BUTTON(widget),.5f,.5f);
  GtkWidget *child=gtk_bin_get_child(GTK_BIN(widget));
  if(GTK_IS_LABEL(child) && !g_object_get_data(G_OBJECT(child),"xp-centered-button-label")) {
   gtk_widget_set_halign(child,GTK_ALIGN_FILL);gtk_widget_set_valign(child,GTK_ALIGN_FILL);
   g_signal_connect(child,"draw",G_CALLBACK(xp_button_label_draw),NULL);
   g_object_set_data(G_OBJECT(child),"xp-centered-button-label",GINT_TO_POINTER(1));
  }
 }
 return TRUE;
}
static gboolean xp_font_settings_refresh(gpointer data) {
 GtkSettings *settings=data;int current=0,wanted=xp_font_dpi()*1024;
 g_object_get(settings,"gtk-xft-dpi",&current,NULL);
 if(current!=wanted)g_object_set(settings,"gtk-xft-dpi",wanted,NULL);
 return G_SOURCE_CONTINUE;
}
static void xp_font_settings_watch(GtkSettings *settings) {
 if(!settings || g_object_get_data(G_OBJECT(settings),"xp-font-settings-watch"))return;
 g_object_set_data(G_OBJECT(settings),"xp-font-settings-watch",GINT_TO_POINTER(1));
 /* GTK modules initialize before the widget class has registered its signals. */
 gpointer widget_class=g_type_class_ref(GTK_TYPE_WIDGET);
 g_signal_add_emission_hook(g_signal_lookup("realize",GTK_TYPE_WIDGET),0,xp_button_center,NULL,NULL);
 g_type_class_unref(widget_class);
 /* Match the selected unsmoothed preview without changing font metrics. */
 g_object_set(settings,"gtk-xft-antialias",0,"gtk-xft-hinting",1,
  "gtk-xft-hintstyle","hintfull","gtk-xft-rgba","none",NULL);
 xp_font_settings_refresh(settings);g_timeout_add_full(G_PRIORITY_DEFAULT,500,xp_font_settings_refresh,g_object_ref(settings),g_object_unref);
}
#endif
