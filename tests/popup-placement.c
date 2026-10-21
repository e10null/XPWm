#define main dwl_main
#include "../compositor/dwl.c"
#undef main
#include <assert.h>
static struct wl_event_source *timer;
static int attempts,clicked,passed;
static int step(void *data)
{
 Client *c,*parent;struct wlr_xdg_client *xc;struct wlr_xdg_surface *s;int x,y,w,h;
 char *path=g_build_filename(g_getenv("XDG_RUNTIME_DIR"),"popup-anchor",NULL);FILE *f=fopen(path,"r");g_free(path);
 (void)data;assert(++attempts<60);
 if(!f)goto again;
 int fields=fscanf(f,"%d %d %d %d",&x,&y,&w,&h);fclose(f);if(fields!=4)goto again;
 wl_list_for_each(c,&clients,link)if(client_get_title(c) && !strcmp(client_get_title(c),"Display Properties")) {
  if(!clicked) {
   struct wlr_box moved=c->geom;moved.x=300;moved.y=150;resize(c,moved,1);
   struct wlr_pointer_button_event event={.button=BTN_LEFT,.state=WL_POINTER_BUTTON_STATE_PRESSED,.time_msec=1000};
   wlr_cursor_warp_closest(cursor,NULL,c->geom.x+c->bw+x+w/2,c->geom.y+c->bw+XP_CAPTION+y+h/2);
   motionnotify(1000,NULL,0,0,0,0);buttonpress(NULL,&event);wlr_seat_pointer_notify_frame(seat);
   event.state=WL_POINTER_BUTTON_STATE_RELEASED;buttonpress(NULL,&event);wlr_seat_pointer_notify_frame(seat);clicked=1;goto again;
  }
  wl_list_for_each(xc,&xdg_shell->clients,link)wl_list_for_each(s,&xc->surfaces,link) {
   if(s->role!=WLR_XDG_SURFACE_ROLE_POPUP || !s->surface->mapped)continue;
   parent=NULL;toplevel_from_wlr_surface(s->surface,&parent,NULL);assert(parent==c);
   struct wlr_scene_tree *tree=s->surface->data;int px,py;wlr_scene_node_coords(&tree->node,&px,&py);
   assert(abs(px-(c->geom.x+(int)c->bw+x))<=12);
   /* GTK menu may align its selected row with the field instead of below it. */
   assert(abs(py-(c->geom.y+(int)c->bw+XP_CAPTION+y))<=h+12);
   path=g_build_filename(g_getenv("XDG_RUNTIME_DIR"),"popup-passed",NULL);assert(g_file_set_contents(path,"PASS: desktop Position dropdown stays attached to its field after moving the dialog\n",-1,NULL));g_free(path);
   passed=1;wl_display_terminate(dpy);return 0;
  }
 }
again:wl_event_source_timer_update(timer,100);return 0;
}
int main(void)
{setup();wlr_seat_set_capabilities(seat,WL_SEAT_CAPABILITY_POINTER|WL_SEAT_CAPABILITY_KEYBOARD);timer=wl_event_loop_add_timer(event_loop,step,NULL);wl_event_source_timer_update(timer,100);run("XPWM_ALLOW_NO_WALLPAPER=1 ./shell/desktop-popup-test");assert(passed);wl_event_source_remove(timer);cleanup();return 0;}
