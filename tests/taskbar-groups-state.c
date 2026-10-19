#define main dwl_original_main
#include "../compositor/dwl.c"
#undef main
#include <assert.h>
static struct wl_event_source *timer;
static int phase, attempts, group_x;
static int marked(const char *name)
{
 char path[4096]; snprintf(path, sizeof(path), "%s/%s", getenv("XDG_RUNTIME_DIR"), name); return access(path, F_OK) == 0;
}
static void click(uint32_t button)
{
 struct wlr_pointer_button_event event = {.button=button, .state=WL_POINTER_BUTTON_STATE_PRESSED, .time_msec=1000+phase*1000};
 wlr_cursor_warp_closest(cursor, NULL, group_x, selmon->m.height-15);
 motionnotify(event.time_msec, NULL, 0, 0, 0, 0); buttonpress(NULL, &event); wlr_seat_pointer_notify_frame(seat);
 event.state = WL_POINTER_BUTTON_STATE_RELEASED; buttonpress(NULL, &event); wlr_seat_pointer_notify_frame(seat);
}
static int tick(void *data)
{
 assert(++attempts < 150);
 if (!selmon) goto again;
 switch (phase) {
 case 0: {
  struct wlr_output_state state; wlr_output_state_init(&state);
  wlr_output_state_set_custom_mode(&state, 640, 480, 60000);
  assert(wlr_output_commit_state(selmon->wlr_output, &state)); wlr_output_state_finish(&state); phase++; break;
 }
 case 1:
  if (marked("group-click")) {
   char path[4096]; snprintf(path, sizeof(path), "%s/group-position", getenv("XDG_RUNTIME_DIR"));
   FILE *file = fopen(path, "r"); assert(file); assert(fscanf(file, "%d", &group_x) == 1); fclose(file);
   assert(selmon->m.width == 640); click(BTN_LEFT); phase++;
  } break;
 case 2: if (marked("group-restore-click")) { click(BTN_RIGHT); phase++; } break;
 case 3: if (marked("group-close-click")) { click(BTN_RIGHT); phase++; } break;
 }
again:
 wl_event_source_timer_update(timer, 100); return 0;
}
int main(void)
{
 setup(); wlr_seat_set_capabilities(seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
 timer = wl_event_loop_add_timer(event_loop, tick, NULL); wl_event_source_timer_update(timer, 100);
 run("sh tests/taskbar-groups.sh client"); assert(phase == 4 && marked("group-passed"));
 wl_event_source_remove(timer); cleanup(); return 0;
}
