#define main dwl_original_main
#include "../compositor/dwl.c"
#undef main
#include <assert.h>
static struct wl_event_source *timer;
static int phase,attempts;
static char marker[1024],release[1024];
static void release_lock(void) {FILE *f=fopen(release,"w");assert(f);fclose(f);}
static int step(void *data) {
 FILE *f;long pid;XpSaverConfig config={.mode=XP_SAVER_NONE,.minutes=1,.text="",.pictures="",.lock=TRUE,.lock_minutes=1};
 (void)data;assert(++attempts<120);
 switch(phase) {
 case 0:
  assert(keybinding(MODKEY|WLR_MODIFIER_SHIFT,XKB_KEY_L));phase++;break;
 case 1:
  if(!locked || !(f=fopen(marker,"r")))break;
  assert(cur_lock && locked_bg->node.enabled && !seat->keyboard_state.focused_surface);
  assert(fscanf(f,"%ld",&pid)==1);fclose(f);assert(kill(pid,SIGKILL)==0);unlink(marker);phase++;break;
 case 2:
  if(cur_lock)break;
  assert(locked && locked_bg->node.enabled); /* Locker crash must fail closed. */
  assert(!keybinding(0,XKB_KEY_NoSymbol));
  /* Explicit recovery locker uses the protocol, then performs authenticated-unlock equivalent. */
  spawn(&(Arg)SHCMD("exec sh \"$XPWM_PROJECT_ROOT/tools/lock.sh\""));phase++;break;
 case 3:
  if(!cur_lock || access(marker,F_OK))break;
  assert(locked);release_lock();phase++;break;
 case 4:
  if(locked)break;
  assert(!cur_lock && !locked_bg->node.enabled);unlink(marker);unlink(release);
  assert(xp_saver_config_save(&config,NULL));screensaver_tick();
  wlr_idle_notifier_v1_set_inhibited(idle_notifier,TRUE);
  screensaver_last_activity=g_get_monotonic_time()-61*G_USEC_PER_SEC;screensaver_tick();assert(!autolock_pid);
  wlr_idle_notifier_v1_set_inhibited(idle_notifier,FALSE);
  screensaver_last_activity=g_get_monotonic_time()-61*G_USEC_PER_SEC;screensaver_tick();assert(autolock_pid && !screensaver_pid);phase++;break;
 case 5:
  if(!locked || access(marker,F_OK))break;
  screensaver_activity();assert(locked && autolock_pid && cur_lock);
  release_lock();phase++;break;
 case 6:
  if(locked || autolock_pid)break;
  unlink(marker);unlink(release);
  config.mode=XP_SAVER_STARS;config.minutes=1;config.lock_minutes=2;
  assert(xp_saver_config_save(&config,NULL));screensaver_tick();
  screensaver_last_activity=g_get_monotonic_time()-61*G_USEC_PER_SEC;screensaver_tick();
  assert(screensaver_pid && !autolock_pid);phase++;break;
 case 7:
  if(!exclusive_focus)break; /* Native saver is displaying on the outputs. */
  screensaver_last_activity=g_get_monotonic_time()-121*G_USEC_PER_SEC;screensaver_tick();
  assert(autolock_pid && screensaver_pid); /* Never dismiss saver before lock acquisition. */
  assert(screensaver_activity());assert(autolock_pid && screensaver_pid);
  phase++;break;
 case 8:
  if(!locked || screensaver_pid || access(marker,F_OK))break;
  assert(cur_lock && locked_bg->node.enabled);
  screensaver_activity();assert(locked && cur_lock);release_lock();phase++;break;
 case 9:
  if(locked || autolock_pid)break;
  assert(!screensaver_pid);
  screensaver_tick();assert(!screensaver_pid && !autolock_pid);
  fprintf(stderr,"PASS: shortcut, fail-closed locker crash, protocol recovery/unlock, independent saver/lock timers, secure saver-to-lock handoff, wake input cannot unlock, timers reset after unlock\n");
  phase++;wl_display_terminate(dpy);return 0;
 }
 wl_event_source_timer_update(timer,100);return 0;
}
int main(void) {
 snprintf(marker,sizeof marker,"%s/lock-acquired",getenv("XDG_RUNTIME_DIR"));snprintf(release,sizeof release,"%s/lock-release",getenv("XDG_RUNTIME_DIR"));
 setup();timer=wl_event_loop_add_timer(event_loop,step,NULL);wl_event_source_timer_update(timer,100);
 run("sleep 20");assert(phase==10);wl_event_source_remove(timer);cleanup();return 0;
}
