#define main screenshot_main
#include "../shell/screenshot.c"
#undef main
#include <glib/gstdio.h>
static int phase;
static gboolean select_test(gpointer data)
{
 GtkWidget *window=g_ptr_array_index(overlays,0);GdkRectangle *r=g_object_get_data(G_OBJECT(window),"capture-monitor");
 g_assert_cmpuint(overlays->len,==,2);
 if(phase==0) {
  GdkEventButton press={.button=1,.type=GDK_BUTTON_PRESS,.x=10,.y=20};select_button(window,&press,r);
  GdkEventMotion motion={.x=110,.y=80};select_motion(window,&motion,r);
  press.type=GDK_BUTTON_RELEASE;select_button(window,&press,r);
 } else {GdkEventKey escape={.keyval=GDK_KEY_Escape};select_key(window,&escape,NULL);}
 return G_SOURCE_REMOVE;
}
int main(int argc,char **argv)
{
 gtk_init(&argc,&argv);luna_init();
 GdkPixbuf *file=gdk_pixbuf_new_from_file(argv[1],NULL);g_assert_nonnull(file);
 GError *error=NULL;screen=snapshot(&desktop,&screen_scale,&error);g_assert_no_error(error);g_assert_nonnull(screen);
 g_assert_cmpint(gdk_pixbuf_get_width(file),==,desktop.width*screen_scale);
 g_assert_cmpint(gdk_pixbuf_get_height(file),==,desktop.height*screen_scale);g_object_unref(file);
 g_timeout_add(100,select_test,NULL);GdkPixbuf *area=select_area();g_assert_nonnull(area);
 g_assert_cmpint(gdk_pixbuf_get_width(area),==,100*screen_scale);g_assert_cmpint(gdk_pixbuf_get_height(area),==,60*screen_scale);g_object_unref(area);
 phase=1;accepted=selecting=FALSE;g_timeout_add(100,select_test,NULL);area=select_area();g_assert_null(area);
 /* Receive PNG data from a separate screenshot client with no keyboard focus. */
 GtkClipboard *clipboard=gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);GdkPixbuf *clip=NULL;
 gint64 deadline=g_get_monotonic_time()+3*G_USEC_PER_SEC;
 while(!clip && g_get_monotonic_time()<deadline) {
  while(g_main_context_iteration(NULL,FALSE));
  clip=gtk_clipboard_wait_for_image(clipboard);if(!clip)g_usleep(10000);
 }
 g_assert_nonnull(clip);g_assert_cmpint(gdk_pixbuf_get_width(clip),==,desktop.width*screen_scale);
 g_assert_cmpint(gdk_pixbuf_get_height(clip),==,desktop.height*screen_scale);g_object_unref(clip);
 gtk_clipboard_set_text(clipboard,"replacement",-1);
 while(g_main_context_iteration(NULL,FALSE));
 char *marker=g_build_filename(g_getenv("XDG_RUNTIME_DIR"),"passed",NULL);
 g_assert_true(g_file_set_contents(marker,"PASS: native two-monitor PNG capture, frozen area selection, Escape cancellation and image clipboard\n",-1,NULL));
 g_free(marker);g_object_unref(screen);luna_finish();return 0;
}
