#define main dwl_original_main
#include "../compositor/dwl.c"
#undef main
#include <assert.h>
static struct wl_event_source *test_timer;
static int attempts, hidden_checked, expanded_checked, normal_checked;
static int marker(const char *name, int write)
{
 char path[4096]; FILE *file;
 snprintf(path, sizeof(path), "%s/%s", getenv("XDG_RUNTIME_DIR"), name);
 if (!write) return access(path, F_OK) == 0;
 file = fopen(path, "w"); assert(file); fputs("passed", file); fclose(file); return 1;
}
static int step(void *data)
{
 LayerSurface *bar = NULL, *l;
 int layer;
 assert(++attempts < 150);
 if (!selmon) goto again;
 for (layer = 0; layer < 4; layer++) wl_list_for_each(l, &selmon->layers[layer], link)
  if (l->mapped && !strcmp(l->layer_surface->namespace, "xpwm-taskbar")) bar = l;
 if (!bar) goto again;
 if (!hidden_checked && marker("settings-hidden", 0)) {
  assert(bar->layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM);
  assert(bar->geom.y == selmon->m.y+selmon->m.height-2);
  assert(selmon->w.height == selmon->m.height-2);
  hidden_checked = 1;
  wlr_cursor_warp_closest(cursor, NULL, selmon->m.x+40, selmon->m.y+selmon->m.height-1);
  motionnotify(1, NULL, 0, 0, 0, 0);
 }
 if (hidden_checked && !expanded_checked && bar->geom.y == selmon->m.y+selmon->m.height-30) {
  assert(selmon->w.height == selmon->m.height-2);
  expanded_checked = 1; marker("settings-expanded-geometry", 1);
 }
 if (!normal_checked && marker("settings-restored", 0) && bar->layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_TOP && selmon->w.height == selmon->m.height-30) {
  normal_checked = 1; marker("settings-normal-geometry", 1);
  fprintf(stderr, "PASS: compositor confirms 2px auto-hide edge, real pointer reveal, unchanged expanded work area and restored 30px taskbar reservation\n");
 }
again:
 wl_event_source_timer_update(test_timer, 100); return 0;
}
int main(void)
{
 setup(); wlr_seat_set_capabilities(seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
 test_timer = wl_event_loop_add_timer(event_loop, step, NULL); wl_event_source_timer_update(test_timer, 100);
 run("sh tests/taskbar-settings.sh client");
 assert(hidden_checked && expanded_checked && normal_checked && marker("settings-passed", 0));
 wl_event_source_remove(test_timer); cleanup(); return 0;
}
