/* Readiness observer for the normal compositor/session, used only by profiling. */
#define main dwl_original_main
#include "../compositor/dwl.c"
#undef main
#include <assert.h>
static struct wl_event_source *profile_timer;
static struct timespec profile_start;
static int profile_attempts, mode_set;
static int profile_ready(void *data)
{
 LayerSurface *l;
 int layer, desktop = 0, taskbar = 0;
 if (++profile_attempts > 1000) die("session profile: shell readiness timeout");
 if (!selmon) goto again;
 if (!mode_set) {
  struct wlr_output_state state;
  wlr_output_state_init(&state); wlr_output_state_set_custom_mode(&state, 1920, 1080, 60000);
  assert(wlr_output_commit_state(selmon->wlr_output, &state)); wlr_output_state_finish(&state); mode_set = 1;
 }
 for (layer = 0; layer < 4; layer++) wl_list_for_each(l, &selmon->layers[layer], link) {
  if (!l->mapped) continue;
  if (!strcmp(l->layer_surface->namespace, "xpwm-desktop")) desktop = 1;
  if (!strcmp(l->layer_surface->namespace, "xpwm-taskbar")) taskbar = 1;
 }
 if ((desktop && taskbar) || getenv("XPWM_PROFILE_BARE")) {
  struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
  double ms = (now.tv_sec-profile_start.tv_sec)*1000. + (now.tv_nsec-profile_start.tv_nsec)/1000000.;
  FILE *file = fopen(getenv("XPWM_PROFILE_READY"), "w"); if (!file) die("session profile: readiness output");
  fprintf(file, "{\"startup_ms\":%.3f,\"width\":%d,\"height\":%d}\n", ms, selmon->m.width, selmon->m.height); fclose(file);
  return 0; /* The observer stops polling once both shell buffers are mapped. */
 }
again:
 wl_event_source_timer_update(profile_timer, 10); return 0;
}
int main(void)
{
 clock_gettime(CLOCK_MONOTONIC, &profile_start); setup();
 profile_timer = wl_event_loop_add_timer(event_loop, profile_ready, NULL); wl_event_source_timer_update(profile_timer, 10);
 run(getenv("XPWM_PROFILE_BARE") ? NULL : "sh shell/session.sh");
 wl_event_source_remove(profile_timer); cleanup(); return 0;
}
