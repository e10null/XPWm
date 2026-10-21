#include "../shell/appearance-settings.h"
#include <pango/pangocairo.h>
static void settle(void) {
 gint64 until=g_get_monotonic_time()+800000;
 while(g_get_monotonic_time()<until){while(g_main_context_iteration(NULL,FALSE));g_usleep(1000);}
}
int main(int argc,char **argv) {
 gtk_init(&argc,&argv);GError *error=NULL;g_assert_cmpint(xp_font_dpi_read(),==,96);
 g_assert_false(xp_font_dpi_save(192,&error));g_assert_nonnull(error);g_clear_error(&error);
 GtkWidget *dialog=gtk_dialog_new(),*page=appearance_page(dialog);
 GtkWidget *sample=gtk_label_new("DPI sample");
 gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),sample);
 gtk_widget_show_all(dialog);settle();
 PangoLayout *layout=gtk_widget_create_pango_layout(sample,"DPI sample");int normal_height;
 pango_layout_get_pixel_size(layout,NULL,&normal_height);g_object_unref(layout);
 g_assert_false(appearance_dirty(page));
 GtkWidget *combo=g_object_get_data(G_OBJECT(page),"font-dpi-combo");
 gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo),"120");g_assert_true(appearance_dirty(page));
 g_object_ref_sink(page);g_object_unref(page);gtk_widget_destroy(dialog);
 g_assert_cmpint(xp_font_dpi_read(),==,96); /* Cancel never persists the draft. */
 dialog=gtk_dialog_new();page=appearance_page(dialog);g_object_ref_sink(page);
 combo=g_object_get_data(G_OBJECT(page),"font-dpi-combo");gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo),"144");
 g_assert_true(xp_font_dpi_save(appearance_dpi(page),&error));g_assert_no_error(error);
 g_assert_cmpint(xp_font_dpi_read(),==,144);g_assert_false(appearance_dirty(page));
 sample=gtk_label_new("DPI sample");
 gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),sample);
 gtk_widget_show_all(dialog);
 xp_font_settings_watch(gtk_settings_get_default());settle();
 layout=gtk_widget_create_pango_layout(sample,"DPI sample");int large_height;
 pango_layout_get_pixel_size(layout,NULL,&large_height);g_object_unref(layout);
 g_assert_cmpint(large_height,>,normal_height);
 int dpi;g_object_get(gtk_settings_get_default(),"gtk-xft-dpi",&dpi,NULL);g_assert_cmpint(dpi,==,144*1024);
 g_assert_true(xp_font_dpi_save(120,&error));g_assert_no_error(error);settle();
 g_object_get(gtk_settings_get_default(),"gtk-xft-dpi",&dpi,NULL);g_assert_cmpint(dpi,==,120*1024);
 g_assert_cmpint(xp_font_dpi_read(),==,120);
 g_assert_true(xp_font_dpi_save(96,&error));g_assert_no_error(error);settle();
 g_object_get(gtk_settings_get_default(),"gtk-xft-dpi",&dpi,NULL);g_assert_cmpint(dpi,==,96*1024);
 g_object_unref(page);gtk_widget_destroy(dialog);
 g_print("PASS: font DPI defaults, validation, Cancel, Apply/persistence and live GTK updates at 96/120/144 DPI\n");return 0;
}
