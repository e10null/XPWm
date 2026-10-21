/* Native Wayland screencopy client and XP-style frozen-screen area selector. */
#define _POSIX_C_SOURCE 200809L
#include "screencopy-client.h"
#include "data-control-client.h"
#include "xp-style.h"
#include "../shared/luna.h"
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <gtk-layer-shell.h>
#include <wayland-client.h>
#include <sys/mman.h>
#include <unistd.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

static struct wl_shm *shm;
static struct zwlr_screencopy_manager_v1 *manager;
typedef struct {
 struct wl_display *display;struct wl_event_queue *queue;
 struct wl_buffer *buffer;void *pixels;size_t size;
 uint32_t width,height,stride,format,flags;gboolean done,failed;
} Capture;
static void buffer_info(void *data,struct zwlr_screencopy_frame_v1 *frame,uint32_t format,uint32_t w,uint32_t h,uint32_t stride)
{
 Capture *c=data;int fd;char *path=NULL;
 c->width=w;c->height=h;c->stride=stride;c->format=format;
 if(!w || !h || w>16384 || h>16384 || stride<(uint64_t)w*4 || (uint64_t)stride*h>512*1024*1024 ||
  (format!=WL_SHM_FORMAT_XRGB8888 && format!=WL_SHM_FORMAT_ARGB8888 && format!=WL_SHM_FORMAT_XBGR8888 && format!=WL_SHM_FORMAT_ABGR8888))goto fail;
 c->size=(size_t)stride*h;fd=g_file_open_tmp("xpwm-capture-XXXXXX",&path,NULL);
 if(fd<0)goto fail;
 unlink(path);g_free(path);
 if(ftruncate(fd,c->size)<0){close(fd);goto fail;}
 c->pixels=mmap(NULL,c->size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
 if(c->pixels==MAP_FAILED){c->pixels=NULL;close(fd);goto fail;}
 struct wl_shm_pool *pool=wl_shm_create_pool(shm,fd,c->size);
 c->buffer=wl_shm_pool_create_buffer(pool,0,w,h,stride,format);wl_shm_pool_destroy(pool);close(fd);
 zwlr_screencopy_frame_v1_copy(frame,c->buffer);return;
fail:c->failed=c->done=TRUE;
}
static void frame_flags(void *data,struct zwlr_screencopy_frame_v1 *frame,uint32_t flags)
{((Capture *)data)->flags=flags;}
static void frame_ready(void *data,struct zwlr_screencopy_frame_v1 *frame,uint32_t hi,uint32_t lo,uint32_t ns)
{((Capture *)data)->done=TRUE;}
static void frame_failed(void *data,struct zwlr_screencopy_frame_v1 *frame)
{Capture *c=data;c->failed=c->done=TRUE;}
static const struct zwlr_screencopy_frame_v1_listener frame_listener={.buffer=buffer_info,.flags=frame_flags,.ready=frame_ready,.failed=frame_failed};
static void shm_format(void *data,struct wl_shm *proxy,uint32_t format){}
static const struct wl_shm_listener shm_listener={shm_format};
static void global(void *data,struct wl_registry *registry,uint32_t name,const char *interface,uint32_t version)
{
 if(!strcmp(interface,"wl_shm")){shm=wl_registry_bind(registry,name,&wl_shm_interface,1);wl_shm_add_listener(shm,&shm_listener,NULL);}
 if(!strcmp(interface,"zwlr_screencopy_manager_v1"))manager=wl_registry_bind(registry,name,&zwlr_screencopy_manager_v1_interface,1);
}
static void removed(void *data,struct wl_registry *registry,uint32_t name){}
static const struct wl_registry_listener registry_listener={global,removed};
static gboolean wait_frame(Capture *c)
{
 gint64 deadline=g_get_monotonic_time()+5*G_USEC_PER_SEC;
 while(!c->done) {
  while(wl_display_prepare_read_queue(c->display,c->queue)<0)
   if(wl_display_dispatch_queue_pending(c->display,c->queue)<0)return FALSE;
  if(c->done){wl_display_cancel_read(c->display);break;}
  wl_display_flush(c->display);
  int timeout=MAX(0,(deadline-g_get_monotonic_time())/1000);
  struct pollfd p={.fd=wl_display_get_fd(c->display),.events=POLLIN};
  int result=poll(&p,1,timeout);
  if(result<0 && errno==EINTR){wl_display_cancel_read(c->display);continue;}
  if(result<=0 || !(p.revents&POLLIN)){wl_display_cancel_read(c->display);return FALSE;}
  if(wl_display_read_events(c->display)<0 || wl_display_dispatch_queue_pending(c->display,c->queue)<0)return FALSE;
 }
 return !c->failed;
}
static GdkPixbuf *capture_monitor(struct wl_display *display,struct wl_event_queue *queue,GdkMonitor *monitor)
{
 Capture c={.display=display,.queue=queue};GdkPixbuf *image=NULL;
 struct zwlr_screencopy_frame_v1 *frame=zwlr_screencopy_manager_v1_capture_output(manager,0,gdk_wayland_monitor_get_wl_output(monitor));
 zwlr_screencopy_frame_v1_add_listener(frame,&frame_listener,&c);
 if(wait_frame(&c)) {
  image=gdk_pixbuf_new(GDK_COLORSPACE_RGB,FALSE,8,c.width,c.height);
  if(image)for(uint32_t y=0;y<c.height;y++) {
   uint32_t row=c.flags&ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT ? c.height-1-y : y;
   const uint32_t *src=(const uint32_t *)((char *)c.pixels+(size_t)row*c.stride);
   guchar *dst=gdk_pixbuf_get_pixels(image)+(size_t)y*gdk_pixbuf_get_rowstride(image);
   gboolean bgr=c.format==WL_SHM_FORMAT_XBGR8888 || c.format==WL_SHM_FORMAT_ABGR8888;
   for(uint32_t x=0;x<c.width;x++){uint32_t v=src[x];dst[x*3]=bgr ? v&255 : (v>>16)&255;dst[x*3+1]=(v>>8)&255;dst[x*3+2]=bgr ? (v>>16)&255 : v&255;}
  }
 }
 zwlr_screencopy_frame_v1_destroy(frame);
 if(c.buffer)wl_buffer_destroy(c.buffer);
 if(c.pixels)munmap(c.pixels,c.size);
 return image;
}
static GdkPixbuf *snapshot(GdkRectangle *bounds,int *scale,GError **error)
{
 GdkDisplay *gdk=gdk_display_get_default();struct wl_display *display=gdk_wayland_display_get_wl_display(gdk);
 struct wl_event_queue *queue=wl_display_create_queue(display);
 struct wl_display *wrapper=wl_proxy_create_wrapper(display);wl_proxy_set_queue((struct wl_proxy *)wrapper,queue);
 struct wl_registry *registry=wl_display_get_registry(wrapper);wl_proxy_wrapper_destroy(wrapper);
 wl_registry_add_listener(registry,&registry_listener,NULL);
 GdkPixbuf *result=NULL;*scale=1;int count=gdk_display_get_n_monitors(gdk);
 if(wl_display_roundtrip_queue(display,queue)<0 || !shm || !manager || !count)goto fail;
 for(int i=0;i<count;i++) {
  GdkMonitor *m=gdk_display_get_monitor(gdk,i);GdkRectangle r;gdk_monitor_get_geometry(m,&r);
  if(!i)*bounds=r;else gdk_rectangle_union(bounds,&r,bounds);
  *scale=MAX(*scale,gdk_monitor_get_scale_factor(m));
 }
 if(bounds->width<=0 || bounds->height<=0 || (uint64_t)bounds->width*bounds->height*(*scale)*(*scale)>128*1024*1024)goto fail;
 result=gdk_pixbuf_new(GDK_COLORSPACE_RGB,FALSE,8,bounds->width*(*scale),bounds->height*(*scale));
 if(!result)goto fail;
 gdk_pixbuf_fill(result,0x000000ff);
 for(int i=0;i<count;i++) {
  GdkMonitor *m=gdk_display_get_monitor(gdk,i);GdkRectangle r;gdk_monitor_get_geometry(m,&r);
  GdkPixbuf *image=capture_monitor(display,queue,m);
  if(!image){g_clear_object(&result);goto fail;}
  int x=(r.x-bounds->x)*(*scale),y=(r.y-bounds->y)*(*scale),w=r.width*(*scale),h=r.height*(*scale);
  double sx=(double)w/gdk_pixbuf_get_width(image),sy=(double)h/gdk_pixbuf_get_height(image);
  gdk_pixbuf_composite(image,result,x,y,w,h,x,y,sx,sy,GDK_INTERP_BILINEAR,255);g_object_unref(image);
 }
 goto done;
fail:g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_FAILED,"Unable to capture this Wayland display. Screen capture may be unavailable or blocked.");
done:
 if(manager){zwlr_screencopy_manager_v1_destroy(manager);manager=NULL;}
 if(shm){wl_shm_destroy(shm);shm=NULL;}
 wl_registry_destroy(registry);wl_event_queue_destroy(queue);return result;
}

static GdkPixbuf *screen;
static GdkRectangle desktop,selection;
static int screen_scale,start_x,start_y;
static gboolean selecting,accepted;
static GPtrArray *overlays;
static gboolean select_draw(GtkWidget *widget,cairo_t *cr,gpointer data)
{
 GdkRectangle *r=data;double x=desktop.x-r->x,y=desktop.y-r->y;
 cairo_save(cr);cairo_translate(cr,x,y);cairo_scale(cr,1./screen_scale,1./screen_scale);
 gdk_cairo_set_source_pixbuf(cr,screen,0,0);cairo_paint(cr);cairo_restore(cr);
 cairo_set_source_rgba(cr,0,0,0,.42);cairo_paint(cr);
 if(selecting) {
  cairo_save(cr);cairo_rectangle(cr,selection.x-r->x,selection.y-r->y,selection.width,selection.height);cairo_clip(cr);
  cairo_translate(cr,x,y);cairo_scale(cr,1./screen_scale,1./screen_scale);gdk_cairo_set_source_pixbuf(cr,screen,0,0);cairo_paint(cr);cairo_restore(cr);
  cairo_set_source_rgb(cr,.19,.42,.77);cairo_set_line_width(cr,2);
  cairo_rectangle(cr,selection.x-r->x+.5,selection.y-r->y+.5,selection.width,selection.height);cairo_stroke(cr);
 }
 cairo_set_source_rgb(cr,1,1,1);PangoLayout *text=gtk_widget_create_pango_layout(widget,"Drag to capture an area.  Esc / right-click to cancel.");
 cairo_move_to(cr,20,20);pango_cairo_show_layout(cr,text);g_object_unref(text);return TRUE;
}
static void redraw(void)
{for(guint i=0;i<overlays->len;i++)gtk_widget_queue_draw(g_ptr_array_index(overlays,i));}
static gboolean select_motion(GtkWidget *widget,GdkEventMotion *event,gpointer data)
{
 if(!selecting)return TRUE;
 GdkRectangle *r=data;int x=r->x+event->x,y=r->y+event->y;
 selection=(GdkRectangle){MIN(start_x,x),MIN(start_y,y),abs(x-start_x),abs(y-start_y)};redraw();return TRUE;
}
static gboolean select_button(GtkWidget *widget,GdkEventButton *event,gpointer data)
{
 GdkRectangle *r=data;
 if(event->button==3){gtk_main_quit();return TRUE;}
 if(event->button!=1)return TRUE;
 if(event->type==GDK_BUTTON_PRESS){start_x=r->x+event->x;start_y=r->y+event->y;selection=(GdkRectangle){start_x,start_y,0,0};selecting=TRUE;redraw();}
 else if(selecting){accepted=selection.width>1 && selection.height>1;gtk_main_quit();}
 return TRUE;
}
static gboolean select_key(GtkWidget *widget,GdkEventKey *event,gpointer data)
{if(event->keyval==GDK_KEY_Escape)gtk_main_quit();return TRUE;}
static GdkPixbuf *select_area(void)
{
 GdkDisplay *display=gdk_display_get_default();overlays=g_ptr_array_new();
 for(int i=0;i<gdk_display_get_n_monitors(display);i++) {
  GdkMonitor *monitor=gdk_display_get_monitor(display,i);GdkRectangle *r=g_new0(GdkRectangle,1);gdk_monitor_get_geometry(monitor,r);
  GtkWidget *window=gtk_window_new(GTK_WINDOW_TOPLEVEL),*view=gtk_drawing_area_new();
  g_object_set_data_full(G_OBJECT(window),"capture-monitor",r,g_free);
  gtk_layer_init_for_window(GTK_WINDOW(window));gtk_layer_set_namespace(GTK_WINDOW(window),"xpwm-screenshot");
  gtk_layer_set_monitor(GTK_WINDOW(window),monitor);gtk_layer_set_layer(GTK_WINDOW(window),GTK_LAYER_SHELL_LAYER_OVERLAY);
  gtk_layer_set_keyboard_mode(GTK_WINDOW(window),GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);gtk_layer_set_exclusive_zone(GTK_WINDOW(window),-1);
  for(int edge=0;edge<GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER;edge++)gtk_layer_set_anchor(GTK_WINDOW(window),edge,TRUE);
  gtk_container_add(GTK_CONTAINER(window),view);gtk_widget_add_events(view,GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_POINTER_MOTION_MASK);
  g_signal_connect(view,"draw",G_CALLBACK(select_draw),r);g_signal_connect(view,"motion-notify-event",G_CALLBACK(select_motion),r);
  g_signal_connect(view,"button-press-event",G_CALLBACK(select_button),r);g_signal_connect(view,"button-release-event",G_CALLBACK(select_button),r);
  g_signal_connect(window,"key-press-event",G_CALLBACK(select_key),NULL);xp_style_apply(window);gtk_widget_show_all(window);
  GdkCursor *cursor=gdk_cursor_new_from_name(display,"crosshair");gdk_window_set_cursor(gtk_widget_get_window(window),cursor);g_clear_object(&cursor);
  g_ptr_array_add(overlays,window);
 }
 gtk_main();
 for(guint i=0;i<overlays->len;i++)gtk_widget_destroy(g_ptr_array_index(overlays,i));
 g_ptr_array_unref(overlays);
 /* Commit removal on GTK's Wayland connection before clipboard ownership
  * blocks on a separate connection. Otherwise the frozen selector stays up. */
 while(g_main_context_iteration(NULL,FALSE));
 gdk_display_sync(display);
 GdkRectangle clipped;
 if(!accepted || !gdk_rectangle_intersect(&selection,&desktop,&clipped))return NULL;
 return gdk_pixbuf_new_subpixbuf(screen,(clipped.x-desktop.x)*screen_scale,(clipped.y-desktop.y)*screen_scale,clipped.width*screen_scale,clipped.height*screen_scale);
}
typedef struct {
 struct zwlr_data_control_manager_v1 *manager;struct wl_seat *seat;
 const char *png;gsize length;gboolean done;
} ImageClipboard;
static void seat_capabilities(void *data,struct wl_seat *seat,uint32_t capabilities){}
static const struct wl_seat_listener seat_listener={.capabilities=seat_capabilities};
static void clipboard_global(void *data,struct wl_registry *registry,uint32_t name,const char *interface,uint32_t version)
{
 ImageClipboard *c=data;
 if(!strcmp(interface,"wl_seat") && !c->seat){c->seat=wl_registry_bind(registry,name,&wl_seat_interface,1);wl_seat_add_listener(c->seat,&seat_listener,NULL);}
 if(!strcmp(interface,"zwlr_data_control_manager_v1"))c->manager=wl_registry_bind(registry,name,&zwlr_data_control_manager_v1_interface,1);
}
static void clipboard_send(void *data,struct zwlr_data_control_source_v1 *source,const char *mime,int32_t fd)
{
 ImageClipboard *c=data;
 if(!strcmp(mime,"image/png"))for(gsize sent=0;sent<c->length;) {
  ssize_t count=write(fd,c->png+sent,c->length-sent);
  if(count<0 && errno==EINTR)continue;
  if(count<=0)break;
  sent+=count;
 }
 close(fd);
}
static void clipboard_cancel(void *data,struct zwlr_data_control_source_v1 *source)
{((ImageClipboard *)data)->done=TRUE;}
static const struct zwlr_data_control_source_v1_listener source_listener={clipboard_send,clipboard_cancel};
static void offered_mime(void *data,struct zwlr_data_control_offer_v1 *offer,const char *mime){}
static const struct zwlr_data_control_offer_v1_listener offer_listener={offered_mime};
static void clipboard_offer(void *data,struct zwlr_data_control_device_v1 *device,struct zwlr_data_control_offer_v1 *offer)
{zwlr_data_control_offer_v1_add_listener(offer,&offer_listener,NULL);}
static void clipboard_selection(void *data,struct zwlr_data_control_device_v1 *device,struct zwlr_data_control_offer_v1 *offer)
{if(offer)zwlr_data_control_offer_v1_destroy(offer);}
static void clipboard_finished(void *data,struct zwlr_data_control_device_v1 *device)
{((ImageClipboard *)data)->done=TRUE;}
static const struct zwlr_data_control_device_v1_listener device_listener={.data_offer=clipboard_offer,.selection=clipboard_selection,.finished=clipboard_finished};
static gboolean serve_clipboard(const char *png,gsize length)
{
 struct wl_display *display=wl_display_connect(NULL);if(!display)return FALSE;
 ImageClipboard c={.png=png,.length=length};struct wl_registry *registry=wl_display_get_registry(display);
 const struct wl_registry_listener listener={clipboard_global,removed};wl_registry_add_listener(registry,&listener,&c);
 wl_display_roundtrip(display);
 if(!c.manager || !c.seat){wl_display_disconnect(display);return FALSE;}
 struct zwlr_data_control_device_v1 *device=zwlr_data_control_manager_v1_get_data_device(c.manager,c.seat);
 zwlr_data_control_device_v1_add_listener(device,&device_listener,&c);
 struct zwlr_data_control_source_v1 *source=zwlr_data_control_manager_v1_create_data_source(c.manager);
 zwlr_data_control_source_v1_add_listener(source,&source_listener,&c);
 zwlr_data_control_source_v1_offer(source,"image/png");zwlr_data_control_device_v1_set_selection(device,source);
 signal(SIGPIPE,SIG_IGN);
 while(!c.done && wl_display_dispatch(display)>=0);
 zwlr_data_control_source_v1_destroy(source);zwlr_data_control_device_v1_destroy(device);
 zwlr_data_control_manager_v1_destroy(c.manager);wl_seat_destroy(c.seat);wl_registry_destroy(registry);wl_display_disconnect(display);return TRUE;
}
int main(int argc,char **argv)
{
 gboolean area=FALSE,no_clipboard=FALSE;char *output=NULL;GError *error=NULL;
 GOptionEntry options[]={{"area",'a',0,G_OPTION_ARG_NONE,&area,"Select an area",NULL},{"output",'o',0,G_OPTION_ARG_FILENAME,&output,"PNG destination","FILE"},{"no-clipboard",0,0,G_OPTION_ARG_NONE,&no_clipboard,"Save only",NULL},{NULL,0,0,0,NULL,NULL,NULL}};
 GOptionContext *context=g_option_context_new("— XPWm screenshot");g_option_context_add_main_entries(context,options,NULL);g_option_context_add_group(context,gtk_get_option_group(TRUE));
 if(!g_option_context_parse(context,&argc,&argv,&error))goto fail;
 g_option_context_free(context);luna_init();
 if(!GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default())){g_set_error_literal(&error,G_IO_ERROR,G_IO_ERROR_NOT_SUPPORTED,"Screenshots require a Wayland session.");goto fail;}
 screen=snapshot(&desktop,&screen_scale,&error);if(!screen)goto fail;
 GdkPixbuf *image=area ? select_area() : g_object_ref(screen);
 if(!image){g_object_unref(screen);return 0;}
 if(!output) {
  const char *pictures=g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
  char *fallback=g_build_filename(g_get_home_dir(),"Pictures",NULL);
  char *folder=g_build_filename(pictures ? pictures : fallback,"Screenshots",NULL);g_free(fallback);
  if(g_mkdir_with_parents(folder,0700)<0){g_set_error(&error,G_FILE_ERROR,g_file_error_from_errno(errno),"Unable to create %s: %s",folder,g_strerror(errno));g_free(folder);goto fail;}
  GDateTime *now=g_date_time_new_now_local();char *name=g_date_time_format(now,"Screenshot-%Y-%m-%d-%H%M%S-%f.png");
  output=g_build_filename(folder,name,NULL);g_free(name);g_free(folder);g_date_time_unref(now);
 }
 char *png=NULL;gsize length=0;
 if(!gdk_pixbuf_save_to_buffer(image,&png,&length,"png",&error,NULL))goto fail;
 if(!g_file_set_contents(output,png,length,&error)){g_free(png);goto fail;}
 g_print("%s\n",output);g_clear_object(&image);g_clear_object(&screen);
 if(!no_clipboard && !serve_clipboard(png,length))g_printerr("XPWm: screenshot saved; clipboard service unavailable.\n");
 g_free(png);g_free(output);luna_finish();return 0;
fail:
 g_printerr("XPWm screenshot: %s\n",error ? error->message : "Capture failed");
 if(gdk_display_get_default()) {
  GtkWidget *message=gtk_message_dialog_new(NULL,0,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,"Unable to take screenshot");
  gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(message),"%s",error ? error->message : "Capture failed");xp_style_apply(message);gtk_dialog_run(GTK_DIALOG(message));gtk_widget_destroy(message);
 }
 g_clear_error(&error);g_free(output);return 1;
}
