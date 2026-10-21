#define main dwl_original_main
#include "../compositor/dwl.c"
#undef main
#include <wlr/interfaces/wlr_keyboard.h>
#include <assert.h>
static struct wl_event_source *test_timer;
static int phase,attempts,leaked_keys;
static void events(void *data,enum wl_protocol_logger_type direction,const struct wl_protocol_logger_message *message)
{
 (void)data;
 if(direction==WL_PROTOCOL_LOGGER_EVENT && !strcmp(wl_resource_get_class(message->resource),"wl_keyboard")
   && !strcmp(message->message->name,"key") && message->arguments[2].u==KEY_A)leaked_keys++;
}
static int saver_layers(void)
{
 Monitor *m;LayerSurface *l;int count=0;
 wl_list_for_each(m,&mons,link)wl_list_for_each(l,&m->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY],link)
  if(l->mapped && !strcmp(l->layer_surface->namespace,"xpwm-screensaver"))count++;
 return count;
}
static void idle_now(void)
{screensaver_last_activity=g_get_monotonic_time()-61*G_USEC_PER_SEC;screensaver_tick();}
static int step(void *data)
{
 struct wlr_keyboard_key_event keyevent={.keycode=KEY_A,.state=WL_KEYBOARD_KEY_STATE_PRESSED,.update_state=true,.time_msec=100};
 (void)data;assert(++attempts<130);
 if(wl_list_length(&mons)!=2)goto again;
 switch(phase){
 case 0: {
  XpSaverConfig config={XP_SAVER_STARS,1,"Windows XP","",FALSE,15};GError *error=NULL;
  char *marker=g_build_filename(g_get_user_runtime_dir(),"saver-settings-passed",NULL);
  gboolean ready=g_file_test(marker,G_FILE_TEST_EXISTS);g_free(marker);if(!ready)break;
  assert(!screensaver_pid);assert(xp_saver_config_save(&config,&error));assert(!error);screensaver_tick();
  wlr_idle_notifier_v1_set_inhibited(idle_notifier,TRUE);idle_now();assert(!screensaver_pid);
  wlr_idle_notifier_v1_set_inhibited(idle_notifier,FALSE);locked=1;idle_now();assert(!screensaver_pid);locked=0;
  idle_now();assert(screensaver_pid>0);phase++;break;
 }
 case 1:
  if(saver_layers()!=2)break;
  wlr_keyboard_notify_key(&kb_group->wlr_group->keyboard,&keyevent);
  assert(!screensaver_pid && consumed[KEY_A]);keyevent.state=WL_KEYBOARD_KEY_STATE_RELEASED;
  wlr_keyboard_notify_key(&kb_group->wlr_group->keyboard,&keyevent);assert(!consumed[KEY_A] && !leaked_keys);phase++;break;
 case 2:
  if(saver_layers() || exclusive_focus)break;
  idle_now();assert(screensaver_pid>0);phase++;break;
 case 3: {
  struct wlr_pointer_button_event button={.button=BTN_LEFT,.state=WL_POINTER_BUTTON_STATE_PRESSED,.time_msec=200};
  if(saver_layers()!=2)break;
  buttonpress(NULL,&button);assert(!screensaver_pid && screensaver_button==BTN_LEFT);
  button.state=WL_POINTER_BUTTON_STATE_RELEASED;buttonpress(NULL,&button);assert(!screensaver_button);phase++;break;
 }
 case 4:
  if(saver_layers() || exclusive_focus)break;
  idle_now();assert(screensaver_pid>0);phase++;break;
 case 5:
  if(saver_layers()!=2)break;
  motionnotify(300,NULL,1,1,1,1);assert(!screensaver_pid);phase++;break;
 case 6:
  if(saver_layers())break;
  fprintf(stderr,"PASS: no idle renderer process; two-output idle activation; inhibitor/lock guards; key/click/motion dismissal without input leakage\n");
  phase++;wl_display_terminate(dpy);return 0;
 }
again:wl_event_source_timer_update(test_timer,100);return 0;
}
int main(void)
{
 struct wl_protocol_logger *logger;
 setup();logger=wl_display_add_protocol_logger(dpy,events,NULL);
 wlr_seat_set_capabilities(seat,WL_SEAT_CAPABILITY_POINTER|WL_SEAT_CAPABILITY_KEYBOARD);
 test_timer=wl_event_loop_add_timer(event_loop,step,NULL);wl_event_source_timer_update(test_timer,100);
 run("GDK_BACKEND=wayland ./shell/screensaver-render-test --settings && touch \"$XDG_RUNTIME_DIR/saver-settings-passed\"");assert(phase==7);
 wl_event_source_remove(test_timer);wl_protocol_logger_destroy(logger);cleanup();return 0;
}
