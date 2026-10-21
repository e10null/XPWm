/* Exercise the real XP-rendered PAM locker without submitting credentials. */
#define main dwl_original_main
#include "../compositor/dwl.c"
#undef main
#include <assert.h>
#include <wlr/interfaces/wlr_keyboard.h>
static struct wl_event_source *timer;
static int phase,attempts,pause_ticks;
static char screenshot[1024],typed[1024];
static void key(uint32_t code,enum wl_keyboard_key_state state) {
 struct wlr_keyboard_key_event event={.time_msec=100,.keycode=code,.update_state=true,.state=state};
 wlr_keyboard_notify_key(&kb_group->wlr_group->keyboard,&event);
}
static void tap(uint32_t code) {key(code,WL_KEYBOARD_KEY_STATE_PRESSED);key(code,WL_KEYBOARD_KEY_STATE_RELEASED);}
static int step(void *data) {
 Monitor *m;int surfaces=0;(void)data;assert(++attempts<130);
 wl_list_for_each(m,&mons,link)if(m->lock_surface && m->lock_surface->surface->mapped)surfaces++;
 switch(phase) {
 case 0:
  if(surfaces!=2)break;
  assert(locked && cur_lock);spawn(&(Arg)SHCMD("./shell/xpwm-screenshot --no-clipboard --output \"$XPWM_LOCK_SCREENSHOT\""));phase++;break;
 case 1:
  if(access(screenshot,F_OK))break;
  tap(KEY_A);phase++;break;
 case 2:
  if(++pause_ticks<3)break;
  spawn(&(Arg)SHCMD("./shell/xpwm-screenshot --no-clipboard --output \"$XPWM_LOCK_TYPED\""));phase++;break;
 case 3:
  if(access(typed,F_OK))break;
  tap(KEY_ESC);tap(KEY_ENTER); /* Empty password is ignored, never sent to PAM. */
  {
   struct wlr_pointer_button_event button={.time_msec=200,.button=BTN_LEFT,.state=WL_POINTER_BUTTON_STATE_PRESSED};
   int x=selmon->m.x+selmon->m.width/2+26+74+168+12,y=selmon->m.y+(80+selmon->m.height-74)/2-40+68+12;
   wlr_cursor_warp_closest(cursor,NULL,x,y);motionnotify(200,NULL,0,0,0,0);
   buttonpress(NULL,&button);button.state=WL_POINTER_BUTTON_STATE_RELEASED;buttonpress(NULL,&button);wlr_seat_pointer_notify_frame(seat);
  }
  phase++;pause_ticks=0;break;
 case 4:
  if(++pause_ticks<5)break;
  assert(locked && cur_lock && surfaces==2);
  fprintf(stderr,"PASS: real XP locker mapped on two outputs; masked typing; Escape/empty Enter cannot unlock; captured native rendering without submitting credentials\n");
  phase++;wl_display_terminate(dpy);return 0;
 }
 wl_event_source_timer_update(timer,100);return 0;
}
int main(void) {
 snprintf(screenshot,sizeof screenshot,"%s",getenv("XPWM_LOCK_SCREENSHOT"));snprintf(typed,sizeof typed,"%s",getenv("XPWM_LOCK_TYPED"));
 setup();wlr_seat_set_capabilities(seat,WL_SEAT_CAPABILITY_KEYBOARD|WL_SEAT_CAPABILITY_POINTER);
 timer=wl_event_loop_add_timer(event_loop,step,NULL);wl_event_source_timer_update(timer,100);
 run("sh tools/lock.sh");assert(phase==5);wl_event_source_remove(timer);cleanup();return 0;
}
