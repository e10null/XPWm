#include "../shell/screensaver-view.c"
#include "../shell/screensaver-settings.c"
#include <glib/gstdio.h>

static void settings_test(int argc,char **argv)
{
 gtk_init(&argc,&argv);
 GtkWidget *window=gtk_window_new(GTK_WINDOW_TOPLEVEL),*page=xp_saver_settings_new(NULL,NULL);
 gtk_container_add(GTK_CONTAINER(window),page);gtk_widget_show_all(window);
 SaverSettings *s=g_object_get_data(G_OBJECT(page),"saver-settings");
 g_assert_false(xp_saver_settings_dirty(page));
 gtk_combo_box_set_active(GTK_COMBO_BOX(s->combo),XP_SAVER_STARS);
 g_assert_true(xp_saver_settings_dirty(page));gtk_widget_destroy(window);
 XpSaverConfig c;xp_saver_config_load(&c);g_assert_cmpint(c.mode,==,XP_SAVER_NONE);xp_saver_config_clear(&c);
 window=gtk_window_new(GTK_WINDOW_TOPLEVEL);page=xp_saver_settings_new(NULL,NULL);gtk_container_add(GTK_CONTAINER(window),page);gtk_widget_show_all(window);
 s=g_object_get_data(G_OBJECT(page),"saver-settings");gtk_combo_box_set_active(GTK_COMBO_BOX(s->combo),XP_SAVER_MARQUEE);
 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->lock),TRUE);
 gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->lock_minutes),4);
 gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->minutes),3);gtk_entry_set_text(GTK_ENTRY(s->text),"Native XP");
 GError *error=NULL;g_assert_true(xp_saver_settings_apply(page,&error));g_assert_no_error(error);g_assert_false(xp_saver_settings_dirty(page));
 xp_saver_config_load(&c);g_assert_cmpint(c.mode,==,XP_SAVER_MARQUEE);g_assert_true(c.lock);g_assert_cmpint(c.lock_minutes,==,4);g_assert_cmpint(c.minutes,==,3);g_assert_cmpstr(c.text,==,"Native XP");xp_saver_config_clear(&c);
 SaverView *view=g_object_get_data(G_OBJECT(s->view),"saver-view");g_assert_cmpuint(view->timer,>,0);
 gtk_widget_hide(s->view);g_assert_cmpuint(view->timer,==,0);
 gtk_widget_show(s->view);gtk_combo_box_set_active(GTK_COMBO_BOX(s->combo),XP_SAVER_BLANK);g_assert_cmpuint(view->timer,==,0);
 char *folder=g_dir_make_tmp("xpwm-saver-photo.XXXXXX",NULL),*photo_path=g_build_filename(folder,"sample.png",NULL);
 GdkPixbuf *photo=gdk_pixbuf_new(GDK_COLORSPACE_RGB,FALSE,8,32,24);gdk_pixbuf_fill(photo,0x4488ccff);
 g_assert_true(gdk_pixbuf_save(photo,photo_path,"png",NULL,NULL));g_object_unref(photo);
 gtk_entry_set_text(GTK_ENTRY(s->pictures),folder);gtk_combo_box_set_active(GTK_COMBO_BOX(s->combo),XP_SAVER_PICTURES);
 gint64 deadline=g_get_monotonic_time()+3*G_USEC_PER_SEC;
 while(!view->photo && g_get_monotonic_time()<deadline){while(g_main_context_iteration(NULL,FALSE));g_usleep(1000);}
 g_assert_nonnull(view->photo);g_assert_cmpint(gdk_pixbuf_get_width(view->photo),>,0);
 gtk_combo_box_set_active(GTK_COMBO_BOX(s->combo),XP_SAVER_GENTOO);
 g_assert_true(xp_saver_settings_apply(page,&error));g_assert_no_error(error);
 xp_saver_config_load(&c);g_assert_cmpint(c.mode,==,XP_SAVER_GENTOO);xp_saver_config_clear(&c);
 gtk_widget_destroy(window);g_remove(photo_path);g_rmdir(folder);g_free(photo_path);g_free(folder);
 g_print("PASS: Screen Saver Apply/Cancel, saved wait/text, hidden-preview timers, static Blank and asynchronous photo loading\n");
}
int main(int argc,char **argv)
{
 if(argc>1 && !strcmp(argv[1],"--settings")){settings_test(argc,argv);return 0;}
 luna_init();cairo_surface_t *sheet=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1280,792);cairo_t *contact=cairo_create(sheet);
 cairo_set_source_rgb(contact,.15,.15,.15);cairo_paint(contact);
 for(int mode=1;mode<XP_SAVER_COUNT;mode++) {
  SaverView s={.config={mode,10,"Windows XP","",FALSE,15}};
  if(mode==XP_SAVER_PICTURES){s.photo=gdk_pixbuf_new(GDK_COLORSPACE_RGB,FALSE,8,64,48);gdk_pixbuf_fill(s.photo,0x2466baff);}
  cairo_surface_t *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,640,480);cairo_t *cr=cairo_create(surface);
  draw_effect(&s,cr,640,480,7.4);cairo_surface_flush(surface);
  unsigned char *pixels=cairo_image_surface_get_data(surface);gboolean lit=FALSE;
  for(int i=0;i<640*480;i++)if(pixels[i*4]||pixels[i*4+1]||pixels[i*4+2]){lit=TRUE;break;}
  g_assert_cmpint(lit,==,mode!=XP_SAVER_BLANK);
  char *before=g_compute_checksum_for_data(G_CHECKSUM_SHA256,pixels,640*480*4);
  draw_effect(&s,cr,640,480,8.8);cairo_surface_flush(surface);
  char *after=g_compute_checksum_for_data(G_CHECKSUM_SHA256,pixels,640*480*4);
  if(mode!=XP_SAVER_BLANK && mode!=XP_SAVER_PICTURES)g_assert_cmpstr(before,!=,after);
  g_free(before);g_free(after);
  if(mode==XP_SAVER_PIPES) {
   draw_effect(&s,cr,640,480,7.401);cairo_surface_flush(surface);
   before=g_compute_checksum_for_data(G_CHECKSUM_SHA256,pixels,640*480*4);
   draw_effect(&s,cr,640,480,7.420);cairo_surface_flush(surface);
   after=g_compute_checksum_for_data(G_CHECKSUM_SHA256,pixels,640*480*4);
   g_assert_cmpstr(before,!=,after);g_free(before);g_free(after);
  }
  gint64 started=g_get_monotonic_time();
  for(int frame=0;frame<8;frame++)draw_effect(&s,cr,640,480,9+frame/30.);
  g_print("%-22s %.2f ms/frame at 640x480\n",xp_saver_names[mode],(g_get_monotonic_time()-started)/8000.);
  if(mode==XP_SAVER_PIPES) {
   cairo_surface_t *large=cairo_image_surface_create(CAIRO_FORMAT_RGB24,3840,2160);
   cairo_t *large_cr=cairo_create(large);gint64 begin=g_get_monotonic_time();
   for(int frame=0;frame<4;frame++)draw_effect(&s,large_cr,3840,2160,17+frame/30.);
   g_assert_cmpint(cairo_image_surface_get_width(s.frame),<=,1280);
   g_assert_cmpint(cairo_image_surface_get_width(s.pipes_cache),<=,960);
   g_print("Pipes at 3840x2160: %.2f ms/frame; bounded render caches\n",(g_get_monotonic_time()-begin)/4000.);
   cairo_destroy(large_cr);cairo_surface_destroy(large);
  }
  int x=((mode-1)%4)*320,y=((mode-1)/4)*264;
  cairo_save(contact);cairo_translate(contact,x,y);cairo_save(contact);cairo_rectangle(contact,0,0,320,240);cairo_clip(contact);draw_effect(&s,contact,320,240,7.4);cairo_restore(contact);
  cairo_set_source_rgb(contact,1,1,1);text(contact,xp_saver_names[mode],13,8,243);cairo_restore(contact);
  cairo_destroy(cr);cairo_surface_destroy(surface);g_clear_object(&s.photo);g_clear_object(&s.ascii);g_clear_object(&s.ascii_light);if(s.pipes_cache)cairo_surface_destroy(s.pipes_cache);if(s.frame)cairo_surface_destroy(s.frame);
 }
 g_assert_cmpint(cairo_surface_write_to_png(sheet,"/tmp/xpwm-screensaver-previews.png"),==,CAIRO_STATUS_SUCCESS);
 cairo_destroy(contact);cairo_surface_destroy(sheet);luna_finish();
 g_print("PASS: all native screen savers render; animated choices change frames; previews saved\n");return 0;
}
