#define main dwl_original_main
#include "../compositor/dwl.c"
#undef main
#include <wlr/interfaces/wlr_keyboard.h>
#include <assert.h>
static struct wl_event_source *test_timer;
static int phase, attempts;
static Client *first, *second, *closing;
static int observe_shortcut, leaked_tab, plain_keys, pointer_enters;
static void protocol_event(void *data, enum wl_protocol_logger_type direction,
 const struct wl_protocol_logger_message *message)
{
 const char *interface = wl_resource_get_class(message->resource);
 (void)data;
 if (direction != WL_PROTOCOL_LOGGER_EVENT || !observe_shortcut) return;
 if (!strcmp(interface, "wl_pointer") && !strcmp(message->message->name, "enter")) pointer_enters++;
 if (strcmp(interface, "wl_keyboard")) return;
 if (!strcmp(message->message->name, "key")) {
  uint32_t key = message->arguments[2].u;
  if (key == KEY_TAB || key == KEY_ESC) leaked_tab++;
  if (key == KEY_A) plain_keys++;
 } else if (!strcmp(message->message->name, "enter")) {
  uint32_t *key;
  wl_array_for_each(key, message->arguments[2].a)
   if (*key == KEY_TAB || *key == KEY_ESC) leaked_tab++;
 }
}
static void send_key(uint32_t code, enum wl_keyboard_key_state state)
{
 struct wlr_keyboard_key_event event = {.time_msec=100, .keycode=code,
  .update_state=true, .state=state};
 wlr_keyboard_notify_key(&kb_group->wlr_group->keyboard, &event);
}
static void modifiers(uint32_t mask);
static void shortcut_regression(void)
{
 struct wlr_keyboard *keyboard = &kb_group->wlr_group->keyboard;
 xkb_mod_index_t logo = xkb_keymap_mod_get_index(keyboard->keymap, XKB_MOD_NAME_LOGO);
 struct wlr_pointer_button_event release = {.button=BTN_LEFT,
  .state=WL_POINTER_BUTTON_STATE_RELEASED, .time_msec=200};
 observe_shortcut = 1;
 focusclient(first, 1);
 wlr_keyboard_notify_modifiers(keyboard, 1u << logo, 0, 0, 0);
 send_key(KEY_TAB, WL_KEYBOARD_KEY_STATE_PRESSED);
 assert(focustop(selmon) == second && consumed[KEY_TAB]);
 send_key(KEY_TAB, WL_KEYBOARD_KEY_STATE_RELEASED);
 assert(!consumed[KEY_TAB] && !leaked_tab);
 wlr_keyboard_notify_modifiers(keyboard, 0, 0, 0, 0);
 send_key(KEY_A, WL_KEYBOARD_KEY_STATE_PRESSED);
 send_key(KEY_A, WL_KEYBOARD_KEY_STATE_RELEASED);
 assert(plain_keys == 2 && !consumed[KEY_A]);
 modifiers(WLR_MODIFIER_ALT);
 send_key(KEY_TAB, WL_KEYBOARD_KEY_STATE_PRESSED);
 assert(switch_scene && consumed[KEY_TAB]);
 send_key(KEY_TAB, WL_KEYBOARD_KEY_STATE_RELEASED);
 send_key(KEY_ESC, WL_KEYBOARD_KEY_STATE_PRESSED);
 assert(!switch_scene);
 send_key(KEY_ESC, WL_KEYBOARD_KEY_STATE_RELEASED);
 assert(!consumed[KEY_TAB] && !consumed[KEY_ESC] && !leaked_tab);
 modifiers(0);
 /* Releasing a Super-drag over content must send a fresh pointer enter,
  * allowing the client to replace the compositor's move cursor. */
 focusclient(first, 1);
 wlr_cursor_warp_closest(cursor, NULL, first->geom.x+50, first->geom.y+70);
 motionnotify(0, NULL, 0, 0, 0, 0);
 moveresize(&(Arg){.ui=CurMove});
 wlr_cursor_warp_closest(cursor, NULL, first->geom.x+50, first->geom.y+70);
 pointer_enters = 0;
 buttonpress(NULL, &release);
 assert(cursor_mode == CurNormal && pointer_enters > 0);
 assert(seat->pointer_state.focused_surface == client_surface(first));
 observe_shortcut = 0;
 fprintf(stderr, "PASS: shortcut presses/releases and enter keys filtered across focus changes, plain typing preserved, overlay cleanup, cursor re-enter after move\n");
}
static void modifiers(uint32_t mask)
{
 struct wlr_keyboard *keyboard = &kb_group->wlr_group->keyboard;
 xkb_mod_index_t index = xkb_keymap_mod_get_index(keyboard->keymap, XKB_MOD_NAME_ALT);
 wlr_keyboard_notify_modifiers(keyboard, mask ? 1u << index : 0, 0, 0, 0);
}
static int step(void *data)
{
 Client *c;
 assert(++attempts < 80);
 switch (phase) {
 case 0:
  if (wl_list_length(&clients) < 2) break;
  first = wl_container_of(clients.next, first, link); second = wl_container_of(clients.prev, second, link);
  shortcut_regression();
  focusclient(first, 1); modifiers(WLR_MODIFIER_ALT);
  assert(keybinding(WLR_MODIFIER_ALT, XKB_KEY_Tab));
  assert(switch_scene && switch_count == 2 && switch_clients[switch_index] == second);
  assert(focustop(selmon) == first); /* Merely browsing must not change focus. */
  if (getenv("XPWM_SWITCHER_SCREENSHOT")) spawn(&(Arg)SHCMD("grim \"$XPWM_SWITCHER_SCREENSHOT\""));
  phase++; break;
 case 1:
  assert(keybinding(WLR_MODIFIER_ALT|WLR_MODIFIER_SHIFT, XKB_KEY_Tab));
  assert(switch_clients[switch_index] == first);
  assert(keybinding(0, XKB_KEY_Escape)); assert(!switch_scene && focustop(selmon) == first);
  assert(keybinding(WLR_MODIFIER_ALT|WLR_MODIFIER_SHIFT, XKB_KEY_ISO_Left_Tab));
  assert(switch_scene && switch_clients[switch_index] == second);
  keybinding(0, XKB_KEY_Escape); modifiers(0);
  xpswitch(&(Arg){.i=1}); assert(switch_scene);
  focusclient(second, 1); assert(!switch_scene);
  system_open(second, second->geom.x, second->geom.y+XP_CAPTION); assert(system_scene);
  focusclient(first, 1); assert(!system_scene);
  system_open(first, first->geom.x, first->geom.y+XP_CAPTION); system_action(1);
  assert(system_moving == first);
  focusclient(second, 1); assert(!system_moving && !grabc);
  focusclient(first, 1);
  xpminimize(second); modifiers(WLR_MODIFIER_ALT);
  assert(keybinding(WLR_MODIFIER_ALT, XKB_KEY_Tab)); assert(switch_clients[switch_index] == second);
  {
   static const xkb_keysym_t tab[] = {XKB_KEY_Tab};
   kb_group->keysyms = tab; kb_group->nsyms = 1; kb_group->mods = WLR_MODIFIER_ALT;
   keyrepeat(kb_group); assert(switch_clients[switch_index] == first);
   keyrepeat(kb_group); assert(switch_clients[switch_index] == second);
  }
  modifiers(0); /* Real wlroots modifier event commits the selection. */
  keyrepeat(kb_group); assert(!switch_scene);
  assert(!switch_scene && !second->minimized && focustop(selmon) == second);
  locked = 1; xpswitch(&(Arg){.i=1}); assert(!switch_scene); locked = 0;
  exclusive_focus = first; xpswitch(&(Arg){.i=1}); assert(!switch_scene); exclusive_focus = NULL;
  fprintf(stderr, "PASS: visual Alt+Tab MRU/reverse selection, Escape cancellation, real Alt-release activation, minimized restoration and lock/modal guards\n");
  modifiers(WLR_MODIFIER_ALT); xpswitch(&(Arg){.i=1});
  closing = switch_clients[switch_index]; client_send_close(closing); phase++; break;
 case 2:
  wl_list_for_each(c, &clients, link) if (c == closing) goto again;
  assert(switch_count == 1 && switch_scene);
  assert(switch_clients[switch_index] != closing);
  modifiers(0); assert(!switch_scene);
  fprintf(stderr, "PASS: closing a window during Alt+Tab removes it safely and preserves selection\n");
  {
   struct wlr_pointer_button_event event = {.button = BTN_LEFT, .state = WL_POINTER_BUTTON_STATE_PRESSED, .time_msec = 1000};
   c = wl_container_of(clients.next, c, link);
   wlr_cursor_warp_closest(cursor, NULL, c->geom.x+c->bw+12, c->geom.y+c->bw+12);
   buttonpress(NULL, &event); event.state = WL_POINTER_BUTTON_STATE_RELEASED; buttonpress(NULL, &event);
   assert(system_scene);
   event.time_msec = 1250; event.state = WL_POINTER_BUTTON_STATE_PRESSED; buttonpress(NULL, &event);
   event.state = WL_POINTER_BUTTON_STATE_RELEASED; buttonpress(NULL, &event);
  }
  phase++; break;
 case 3:
  if (!wl_list_empty(&clients)) break;
  assert(!system_scene && !system_icon_client && !system_moving);
  fprintf(stderr, "PASS: double-clicking a caption icon closes the actual window and clears menu references\n");
  phase++; wl_display_terminate(dpy); return 0;
 }
again:
 wl_event_source_timer_update(test_timer, 200); return 0;
}
int main(void)
{
 struct wl_protocol_logger *logger;
 setup(); logger = wl_display_add_protocol_logger(dpy, protocol_event, NULL);
 wlr_seat_set_capabilities(seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
 test_timer = wl_event_loop_add_timer(event_loop, step, NULL); wl_event_source_timer_update(test_timer, 200);
 run("sh tests/switcher.sh client"); assert(phase == 4); wl_event_source_remove(test_timer); wl_protocol_logger_destroy(logger); cleanup(); return 0;
}
