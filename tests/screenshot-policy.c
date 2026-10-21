/* Real pointer input; selector must unmap while screenshot still owns clipboard. */
#define main dwl_original_main
#include "../compositor/dwl.c"
#undef main
#include <wlr/interfaces/wlr_keyboard.h>
#include <assert.h>
static struct wl_event_source *timer;
static int phase,attempts,closing,cycles;
static LayerSurface *selector;
static int selector_count(void)
{
 Monitor *m;LayerSurface *l;int count=0;
 wl_list_for_each(m,&mons,link)wl_list_for_each(l,&m->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY],link)
  if(l->mapped && !strcmp(l->layer_surface->namespace,"xpwm-screenshot")){count++;selector=l;}
 return count;
}
static int step(void *data)
{
 struct wlr_pointer_button_event button={.button=BTN_LEFT,.state=WL_POINTER_BUTTON_STATE_PRESSED,.time_msec=1000};
 struct wlr_keyboard_key_event key={.keycode=KEY_ESC,.state=WL_KEYBOARD_KEY_STATE_PRESSED,.update_state=true,.time_msec=2000};
 char *path;int count=selector_count();(void)data;if(++attempts>=60){fprintf(stderr,"Screenshot test timed out in phase %d, selectors=%d, cursor=%d\n",phase,count,cursor_mode);abort();}
 switch(phase) {
 case 0:
  if(count!=2 || wl_list_empty(&clients))break;
  wlr_cursor_warp_closest(cursor,NULL,selector->geom.x+10,selector->geom.y+20);
  motionnotify(1000,NULL,0,0,0,0);buttonpress(NULL,&button);wlr_seat_pointer_notify_frame(seat);phase++;break;
 case 1:
  wlr_cursor_warp_closest(cursor,NULL,selector->geom.x+110,selector->geom.y+80);
  motionnotify(1100,NULL,0,0,0,0);wlr_seat_pointer_notify_frame(seat);phase++;break;
 case 2:
  button.state=WL_POINTER_BUTTON_STATE_RELEASED;button.time_msec=1200;buttonpress(NULL,&button);wlr_seat_pointer_notify_frame(seat);phase++;break;
 case 3:
  path=g_build_filename(g_getenv("XDG_RUNTIME_DIR"),"area.png",NULL);
  if(!g_file_test(path,G_FILE_TEST_EXISTS)){g_free(path);break;}g_free(path);
  assert(++closing<15);if(count || exclusive_focus)break;
  if(!seat->selection_source)break; /* clipboard ownership remains live */
  {
   Client *target=wl_container_of(clients.next,target,link);
   focusclient(target,1);
   assert(seat->keyboard_state.focused_surface==client_surface(target));
   motionnotify(1500,NULL,0,0,0,0);
  }
  spawn(&(Arg)SHCMD("./shell/xpwm-screenshot --area --no-clipboard --output \"$XDG_RUNTIME_DIR/cancel.png\""));phase++;break;
 case 4:
  if(count!=2)break;
  wlr_keyboard_notify_key(&kb_group->wlr_group->keyboard,&key);key.state=WL_KEYBOARD_KEY_STATE_RELEASED;
  wlr_keyboard_notify_key(&kb_group->wlr_group->keyboard,&key);phase++;break;
 case 5:
  if(count || exclusive_focus)break;
  path=g_build_filename(g_getenv("XDG_RUNTIME_DIR"),"cancel.png",NULL);assert(!g_file_test(path,G_FILE_TEST_EXISTS));g_free(path);
  if(++cycles<3) {
   path=g_build_filename(g_getenv("XDG_RUNTIME_DIR"),"area.png",NULL);unlink(path);g_free(path);
   phase=attempts=closing=0;
   spawn(&(Arg)SHCMD("./shell/xpwm-screenshot --area --output \"$XDG_RUNTIME_DIR/area.png\""));break;
  }
  path=g_build_filename(g_getenv("XDG_RUNTIME_DIR"),"policy-passed",NULL);assert(g_file_set_contents(path,"PASS: repeated crop, application refocus with live clipboard, and Escape cancellation\n",-1,NULL));g_free(path);
  phase++;wl_display_terminate(dpy);return 0;
 }
 wl_event_source_timer_update(timer,100);return 0;
}
int main(void)
{
 setup();wlr_seat_set_capabilities(seat,WL_SEAT_CAPABILITY_POINTER|WL_SEAT_CAPABILITY_KEYBOARD);
 spawn(&(Arg)SHCMD("foot --title=Screenshot-focus-test"));
 timer=wl_event_loop_add_timer(event_loop,step,NULL);wl_event_source_timer_update(timer,100);
 run("./shell/xpwm-screenshot --area --output \"$XDG_RUNTIME_DIR/area.png\"");assert(phase==6);
 wl_event_source_remove(timer);cleanup();return 0;
}
