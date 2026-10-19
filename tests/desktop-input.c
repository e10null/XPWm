#define main dwl_original_main
#include "../compositor/dwl.c"
#undef main
#include <wlr/interfaces/wlr_keyboard.h>
#include <assert.h>
static struct wl_event_source *timer;
static int phase, attempts, click_x, click_y, drag_wait;
static int marker(const char *name, int write)
{
 char path[4096]; FILE *file;
 snprintf(path, sizeof(path), "%s/%s", getenv("XDG_RUNTIME_DIR"), name);
 if (!write) return access(path, F_OK) == 0;
 file = fopen(path, "w"); assert(file); fputs("passed", file); fclose(file); return 1;
}
static void key(uint32_t code, int control)
{
 struct wlr_keyboard *keyboard = &kb_group->wlr_group->keyboard;
 xkb_mod_index_t index = xkb_keymap_mod_get_index(keyboard->keymap, XKB_MOD_NAME_CTRL);
 struct wlr_keyboard_key_event event = {.keycode=code, .state=WL_KEYBOARD_KEY_STATE_PRESSED, .time_msec=100};
 wlr_keyboard_notify_modifiers(keyboard, control ? 1u<<index : 0, 0, 0, 0);
 keypress(&kb_group->key, &event); event.state = WL_KEYBOARD_KEY_STATE_RELEASED; keypress(&kb_group->key, &event);
 wlr_keyboard_notify_modifiers(keyboard, 0, 0, 0, 0);
}
static void button_at(double x, double y, int pressed)
{
 struct wlr_pointer_button_event event = {.button=BTN_LEFT, .state=pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED, .time_msec=1000+(uint32_t)phase*1000};
 wlr_cursor_warp_closest(cursor, NULL, x, y); motionnotify(event.time_msec, NULL, 0, 0, 0, 0); buttonpress(NULL, &event); wlr_seat_pointer_notify_frame(seat);
}
static int tick(void *data)
{
 Client *c;
 LayerSurface *desktop = NULL, *l;
 if (++attempts >= 180) { fprintf(stderr, "Desktop compositor phase=%d focus=%p\n", phase, (void *)seat->keyboard_state.focused_surface); abort(); }
 if (!selmon) goto again;
 wl_list_for_each(l, &selmon->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND], link)
  if (l->mapped && !strcmp(l->layer_surface->namespace, "xpwm-desktop")) desktop = l;
 if (!desktop) goto again;
 switch (phase) {
 case 0: {
  char path[4096]; FILE *file;
  if (!marker("desktop-target", 0) || wl_list_empty(&clients)) break;
  c = wl_container_of(clients.next, c, link);
  assert(seat->keyboard_state.focused_surface == client_surface(c));
  resize(c, (struct wlr_box){500,60,420,260}, 0);
  snprintf(path, sizeof(path), "%s/desktop-target", getenv("XDG_RUNTIME_DIR")); file = fopen(path, "r"); assert(file);
  assert(fscanf(file, "%d %d", &click_x, &click_y) == 2); fclose(file);
  button_at(click_x, click_y, 1);
  button_at(click_x, click_y, 0);
  assert(seat->keyboard_state.focused_surface == desktop->layer_surface->surface && !exclusive_focus);
  key(KEY_F2, 0); phase++; break;
 }
 case 1: if (marker("desktop-drag", 0) && seat->keyboard_state.focused_surface == desktop->layer_surface->surface) { button_at(click_x, click_y, 1); phase++; } break;
 case 2: wlr_cursor_move(cursor, NULL, 96, 0); motionnotify(2000, NULL, 0, 0, 0, 0); wlr_seat_pointer_notify_frame(seat); phase++; break;
 case 3: if (++drag_wait < 2) break; button_at(click_x+96, click_y, 0); marker("desktop-drag-complete", 1); phase++; break;
 case 4: if (marker("desktop-rubber", 0)) { button_at(4, 4, 1); phase++; } break;
 case 5: wlr_cursor_warp_closest(cursor, NULL, 300, 650); motionnotify(3000, NULL, 0, 0, 0, 0); wlr_seat_pointer_notify_frame(seat); phase++; break;
 case 6: button_at(300, 650, 0); marker("desktop-rubber-complete", 1); phase++; break;
 case 7: if (marker("desktop-copy-ready", 0)) { key(KEY_C, 1); key(KEY_V, 1); phase++; } break;
 case 8: break;
 }
again:
 wl_event_source_timer_update(timer, 100); return 0;
}
int main(void)
{
 setup(); wlr_seat_set_capabilities(seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
 timer = wl_event_loop_add_timer(event_loop, tick, NULL); wl_event_source_timer_update(timer, 100);
 run("sh tests/desktop-input.sh client"); assert(phase == 8 && marker("desktop-input-passed", 0));
 wl_event_source_remove(timer); cleanup(); return 0;
}
