#define main dwl_original_main
#include "../compositor/dwl.c"
#undef main
#include <assert.h>
static struct wl_event_source *timer;
static int phase,attempts;
static char *normal;
static char *caption_hash(Client *c) {
 void *data;uint32_t format;size_t stride;char *hash;
 struct wlr_buffer *buffer=c->caption->buffer;
 assert(wlr_buffer_begin_data_ptr_access(buffer,WLR_BUFFER_DATA_PTR_ACCESS_READ,&data,&format,&stride));
 hash=g_compute_checksum_for_data(G_CHECKSUM_SHA256,data,stride*buffer->height);
 wlr_buffer_end_data_ptr_access(buffer);return hash;
}
static int step(void *data) {
 Client *c;char *hash;(void)data;assert(++attempts<80);
 if(wl_list_empty(&clients))goto again;
 c=wl_container_of(clients.next,c,link);if(!c->caption || !c->caption->buffer)goto again;
 if(phase==0) {normal=caption_hash(c);assert(xp_font_dpi_save(144,NULL));phase++;}
 else if(phase==1 && getenv("QT_FONT_DPI") && !strcmp(getenv("QT_FONT_DPI"),"144")) {
  hash=caption_hash(c);assert(strcmp(normal,hash));g_free(hash);assert(xp_font_dpi_save(96,NULL));phase++;
 } else if(phase==2 && !strcmp(getenv("QT_FONT_DPI"),"96")) {
  hash=caption_hash(c);assert(!strcmp(normal,hash));g_free(hash);g_free(normal);
  fprintf(stderr,"PASS: live compositor caption DPI changes and restores exactly; new Qt clients inherit updated DPI\n");
  phase++;wl_display_terminate(dpy);return 0;
 }
again:wl_event_source_timer_update(timer,100);return 0;
}
int main(void) {
 setup();timer=wl_event_loop_add_timer(event_loop,step,NULL);wl_event_source_timer_update(timer,100);
 run("foot --title='Font scaling caption'");assert(phase==3);wl_event_source_remove(timer);cleanup();return 0;
}
