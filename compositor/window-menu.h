/* Compositor-owned caption menu and keyboard/pointer Move/Size operations. */
static struct wlr_scene_buffer *system_scene;
static Client *system_target, *system_moving;
static int system_row = -1, system_pressed = -1, system_move_kind, system_was_floating;
static struct wlr_box system_origin;
static uint32_t system_swallow, system_icon_time;
static Client *system_icon_client;

static unsigned int
system_enabled(Client *c)
{
 unsigned int mask = (1u << 5);
 if (c->maximized || c->minimized) mask |= 1u;
 if (!c->maximized && !c->minimized) {
  mask |= 1u << 1;
  if (client_resize_edges(c)) mask |= 1u << 2;
 }
 if (!c->minimized) mask |= 1u << 3;
 if (!c->maximized && client_can_maximize(c)) mask |= 1u << 4;
 return mask;
}
static void
system_close(void)
{
 if (system_scene) wlr_scene_node_destroy(&system_scene->node);
 system_scene = NULL; system_target = NULL; system_row = system_pressed = -1;
}
static void
system_stop_move(int cancel)
{
 Client *c = system_moving;
 if (!c) return;
 system_moving = NULL; cursor_mode = CurNormal; resize_edges = 0; grabc = NULL;
 wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
 if (!cancel && c->mon) setmon(c, xytomon(cursor->x, cursor->y), 0);
 if (cancel && client_surface(c)->mapped) {
  setfloating(c, system_was_floating); resize(c, system_origin, 0);
 }
}
static void
system_draw(void)
{
 struct wlr_buffer *buffer;
 if (!system_scene || !system_target) return;
 buffer = xp_system_menu_buffer(system_row, system_enabled(system_target));
 if (!buffer) { system_close(); return; }
 wlr_scene_buffer_set_buffer(system_scene, buffer); wlr_buffer_drop(buffer);
}
static void
system_open(Client *c, int x, int y)
{
 if (!c || locked || exclusive_focus || c->isfullscreen || !c->mon || client_is_unmanaged(c)) return;
 switch_finish(0); system_close(); caption_clicked = NULL;
 system_target = c; cursor_mode = CurNormal;
 system_scene = wlr_scene_buffer_create(layers[LyrOverlay], NULL);
 wlr_scene_node_set_position(&system_scene->node,
  MAX(c->mon->m.x, MIN(x, c->mon->m.x+c->mon->m.width-180)),
  MAX(c->mon->m.y, MIN(y, c->mon->m.y+c->mon->m.height-132)));
 wlr_seat_pointer_clear_focus(seat); system_draw();
}
static void
xpsystemmenu(const Arg *arg)
{
 Client *c = focustop(selmon);
 if (c) system_open(c, c->geom.x, c->geom.y+(int)c->bw+XP_CAPTION);
}
static void
system_action(int row)
{
 Client *c = system_target;
 if (!c || !(system_enabled(c) & (1u << row))) return;
 system_close();
 switch (row) {
 case 0: if (c->maximized) xpmaximize(c, 0); focusclient(c, 1); break;
 case 1: case 2:
  system_moving = c; system_origin = c->geom; system_was_floating = c->isfloating; system_move_kind = row;
  setfloating(c, 1); grabc = c; resize_origin = c->geom;
  resize_edges = client_resize_edges(c) & (WLR_EDGE_RIGHT | WLR_EDGE_BOTTOM);
  if (row == 1) {
   wlr_cursor_warp_closest(cursor, NULL, c->geom.x+c->geom.width/2., c->geom.y+c->bw+12);
   grabcx = (int)round(cursor->x)-c->geom.x; grabcy = (int)round(cursor->y)-c->geom.y;
  } else wlr_cursor_warp_closest(cursor, NULL, c->geom.x+c->geom.width-1, c->geom.y+c->geom.height-1);
  resize_x = cursor->x; resize_y = cursor->y;
  cursor_mode = row == 1 ? CurMove : CurResize;
  wlr_cursor_set_xcursor(cursor, cursor_mgr, row == 1 ? "move" : "se-resize"); break;
 case 3: xpminimize(c); break;
 case 4: xpmaximize(c, 1); break;
 case 5: client_send_close(c); break;
 }
}
static int
system_hit(double x, double y)
{
 int row;
 if (!system_scene) return -1;
 x -= system_scene->node.x; y -= system_scene->node.y;
 if (x < 3 || x >= 177 || y < 3 || y >= 129) return -1;
 if (y >= 109) return 5;
 row = ((int)y-3)/20;
 return row < 5 ? row : -1;
}
static int
system_pointer(struct wlr_pointer_button_event *event)
{
 int row;
 if (event->state == WL_POINTER_BUTTON_STATE_RELEASED && system_swallow == event->button) {
  system_swallow = 0; return 1;
 }
 if (system_moving && event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
  system_stop_move(event->button == BTN_RIGHT); system_swallow = event->button; return 1;
 }
 if (!system_scene) return 0;
 row = system_hit(cursor->x, cursor->y);
 if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
  if (row < 0) {
   Client *c = system_target;
   int icon_double = c == system_icon_client && event->button == BTN_LEFT && event->time_msec-system_icon_time < 500 &&
    cursor->x >= c->geom.x+c->bw+4 && cursor->x < c->geom.x+c->bw+25 &&
    cursor->y >= c->geom.y+c->bw && cursor->y < c->geom.y+c->bw+XP_CAPTION;
   system_close(); if (icon_double) { system_icon_client = NULL; client_send_close(c); }
   system_swallow = event->button; return 1;
  }
  if (event->button == BTN_LEFT) system_pressed = row;
 } else if (event->button == BTN_LEFT && row >= 0 && row == system_pressed) system_action(row);
 return 1;
}
static int
system_key(uint32_t mods, xkb_keysym_t sym)
{
 struct wlr_box box;
 int i, step = mods & WLR_MODIFIER_CTRL ? 1 : 8;
 const xkb_keysym_t shortcuts[] = {XKB_KEY_r, XKB_KEY_m, XKB_KEY_s, XKB_KEY_n, XKB_KEY_x, XKB_KEY_c};
 if (system_moving) {
  if (sym == XKB_KEY_Escape || sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter) system_stop_move(sym == XKB_KEY_Escape);
  else {
   box = system_moving->geom;
   if (system_move_kind == 1) {
    if (sym == XKB_KEY_Left) box.x -= step;
    if (sym == XKB_KEY_Right) box.x += step;
    if (sym == XKB_KEY_Up) box.y -= step;
    if (sym == XKB_KEY_Down) box.y += step;
   } else {
    if (resize_edges & WLR_EDGE_RIGHT) { if (sym == XKB_KEY_Left) box.width -= step; if (sym == XKB_KEY_Right) box.width += step; }
    if (resize_edges & WLR_EDGE_BOTTOM) { if (sym == XKB_KEY_Up) box.height -= step; if (sym == XKB_KEY_Down) box.height += step; }
   }
   resize(system_moving, box, 1); resize_origin = system_moving->geom;
   resize_x = cursor->x; resize_y = cursor->y;
   grabcx = (int)round(cursor->x)-system_moving->geom.x; grabcy = (int)round(cursor->y)-system_moving->geom.y;
  }
  return 1;
 }
 if (!system_scene) return 0;
 if (sym == XKB_KEY_Escape) system_close();
 else if ((mods & WLR_MODIFIER_ALT) && sym == XKB_KEY_F4) system_action(5);
 else if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter) { if (system_row >= 0) system_action(system_row); }
 else if (sym == XKB_KEY_Up || sym == XKB_KEY_Down) {
  if (system_row < 0 && sym == XKB_KEY_Up) system_row = 0;
  for (i = 0; i < 6; i++) {
   system_row = (system_row + 6 + (sym == XKB_KEY_Up ? -1 : 1)) % 6;
   if (system_enabled(system_target) & (1u << system_row)) break;
  }
  system_draw();
 } else for (i = 0; i < 6; i++) if (xkb_keysym_to_lower(sym) == shortcuts[i]) { system_action(i); break; }
 return 1;
}
static void
system_forget(Client *c)
{
 if (system_target == c) system_close();
 if (system_moving == c) { system_moving = NULL; grabc = NULL; cursor_mode = CurNormal; resize_edges = 0; }
 if (system_icon_client == c) system_icon_client = NULL;
}
