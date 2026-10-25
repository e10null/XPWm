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
 int antialias;g_object_get(gtk_settings_get_default(),"gtk-xft-antialias",&antialias,NULL);g_assert_cmpint(antialias,==,0);
 GtkWidget *button=gtk_button_new_with_label("OK");
 gtk_button_set_alignment(GTK_BUTTON(button),0,0);
 gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),button);
 gtk_widget_show_all(dialog);settle();
 gfloat xalign,yalign;gtk_button_get_alignment(GTK_BUTTON(button),&xalign,&yalign);
 g_assert_cmpfloat(xalign,==,.5f);g_assert_cmpfloat(yalign,==,.5f);
 GtkWidget *button_label=gtk_bin_get_child(GTK_BIN(button));
 int width=gtk_widget_get_allocated_width(button_label),height=gtk_widget_get_allocated_height(button_label);
 cairo_surface_t *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,width,height);
 cairo_t *cr=cairo_create(surface);cairo_set_source_rgb(cr,1,1,1);cairo_paint(cr);
 gtk_widget_draw(button_label,cr);cairo_destroy(cr);cairo_surface_flush(surface);
 unsigned char *pixels=cairo_image_surface_get_data(surface);int stride=cairo_image_surface_get_stride(surface);
 int left=width,right=-1,top=height,bottom=-1;
 for(int y=0;y<height;y++)for(int x=0;x<width;x++) {
  uint32_t pixel=((uint32_t *)(pixels+y*stride))[x];
  if((pixel&0xffffff)<0x404040){left=MIN(left,x);right=MAX(right,x);top=MIN(top,y);bottom=MAX(bottom,y);}
 }
 g_assert_cmpint(right,>=,left);g_assert_cmpint(ABS(left+right+1-width),<=,1);
 g_assert_cmpint(ABS(top+bottom+1-height),<=,1);cairo_surface_destroy(surface);
 layout=gtk_widget_create_pango_layout(sample,"DPI sample");int large_height;
 pango_layout_get_pixel_size(layout,NULL,&large_height);g_object_unref(layout);
 g_assert_cmpint(large_height,>,normal_height);
 int dpi;g_object_get(gtk_settings_get_default(),"gtk-xft-dpi",&dpi,NULL);g_assert_cmpint(dpi,==,144*1024);
 g_assert_true(xp_font_dpi_save(120,&error));g_assert_no_error(error);settle();
 g_object_get(gtk_settings_get_default(),"gtk-xft-dpi",&dpi,NULL);g_assert_cmpint(dpi,==,120*1024);
 g_assert_cmpint(xp_font_dpi_read(),==,120);
 g_assert_true(xp_font_dpi_save(96,&error));g_assert_no_error(error);settle();
 g_object_get(gtk_settings_get_default(),"gtk-xft-dpi",&dpi,NULL);g_assert_cmpint(dpi,==,96*1024);
 for(int smaller=84;smaller>=72;smaller-=12) {
  char id[8];g_snprintf(id,sizeof id,"%d",smaller);
  gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo),id);
  g_assert_cmpint(appearance_dpi(page),==,smaller);
  g_assert_true(xp_font_dpi_save(appearance_dpi(page),&error));g_assert_no_error(error);settle();
  g_object_get(gtk_settings_get_default(),"gtk-xft-dpi",&dpi,NULL);g_assert_cmpint(dpi,==,smaller*1024);
  g_assert_cmpint(xp_font_dpi_read(),==,smaller);
  layout=gtk_widget_create_pango_layout(sample,"DPI sample");int small_height;
  pango_layout_get_pixel_size(layout,NULL,&small_height);g_object_unref(layout);
  g_assert_cmpint(small_height,<,normal_height);
 }
 g_object_unref(page);gtk_widget_destroy(dialog);
 g_print("PASS: font DPI defaults, validation, Cancel, Apply/persistence and live GTK updates at 72/84/96/120/144 DPI\n");return 0;
}
