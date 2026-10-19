/* Snapshot MRU order while Alt is held. Focus changes only when committed. */
static Client **switch_clients;
static size_t switch_count, switch_index;
static struct wlr_scene_buffer *switch_scene;
static Monitor *switch_monitor;

static void
switch_finish(int commit)
{
 Client *target = switch_count ? switch_clients[switch_index] : NULL;
 if (switch_scene) wlr_scene_node_destroy(&switch_scene->node);
 switch_scene = NULL;
 free(switch_clients); switch_clients = NULL; switch_count = switch_index = 0;
 switch_monitor = NULL;
 if (commit && target && !locked && !exclusive_focus) focusclient(target, 1);
}

static void
switch_draw(void)
{
 struct wlr_buffer *buffer;
 int columns, width;
 if (!switch_count || !switch_monitor || !switch_monitor->wlr_output->enabled) { switch_finish(0); return; }
 columns = MIN(7, MAX(1, (switch_monitor->w.width-40-24)/44));
 columns = MIN(columns, (int)switch_count);
 width = MIN(switch_monitor->w.width-16, MAX(320, columns*44+24));
 buffer = xp_switcher_buffer(width, columns, (int)switch_count, (int)switch_index,
   client_get_title(switch_clients[switch_index]));
 if (!buffer) { switch_finish(0); return; }
 if (!switch_scene) switch_scene = wlr_scene_buffer_create(layers[LyrOverlay], buffer);
 else wlr_scene_buffer_set_buffer(switch_scene, buffer);
 wlr_scene_node_set_position(&switch_scene->node,
  switch_monitor->w.x + (switch_monitor->w.width-width)/2,
  switch_monitor->w.y + (switch_monitor->w.height-buffer->height)/2);
 wlr_scene_node_raise_to_top(&switch_scene->node); wlr_buffer_drop(buffer);
}

static void
xpswitch(const Arg *arg)
{
 Client *c, *active = NULL;
 size_t i;
 toplevel_from_wlr_surface(seat->keyboard_state.focused_surface, &active, NULL);
 if (locked || exclusive_focus || !selmon || seat->drag) { switch_finish(0); return; }
 if (!switch_count) {
  wl_list_for_each(c, &fstack, flink)
   if (c->mon == selmon && (c->tags & selmon->tagset[selmon->seltags]) && client_surface(c)->mapped && !client_is_unmanaged(c)) switch_count++;
  if (!switch_count) return;
  switch_clients = ecalloc(switch_count, sizeof(*switch_clients));
  switch_index = arg->i < 0 ? 0 : switch_count-1;
  i = 0;
  wl_list_for_each(c, &fstack, flink)
   if (c->mon == selmon && (c->tags & selmon->tagset[selmon->seltags]) && client_surface(c)->mapped && !client_is_unmanaged(c)) {
    switch_clients[i] = c; if (c == active) switch_index = i; i++;
   }
  switch_monitor = selmon;
 }
 switch_index = (switch_index + switch_count + (arg->i < 0 ? -1 : 1)) % switch_count;
 switch_draw();
}

static void
switch_forget(Client *c)
{
 size_t i;
 for (i = 0; i < switch_count; i++) {
  if (switch_clients[i] != c) continue;
  memmove(switch_clients+i, switch_clients+i+1, (switch_count-i-1)*sizeof(*switch_clients));
  switch_count--;
  if (!switch_count) { switch_finish(0); return; }
  if (i < switch_index) switch_index--;
  switch_index %= switch_count; switch_draw(); return;
 }
}

static int
switch_key(uint32_t mods, xkb_keysym_t sym)
{
 if (!switch_scene) return 0;
 if (sym == XKB_KEY_Escape) switch_finish(0);
 else if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter) switch_finish(1);
 else if (sym == XKB_KEY_Tab || sym == XKB_KEY_ISO_Left_Tab || sym == XKB_KEY_Left || sym == XKB_KEY_Right)
  xpswitch(&(Arg){.i = (mods & WLR_MODIFIER_SHIFT) || sym == XKB_KEY_ISO_Left_Tab || sym == XKB_KEY_Left ? -1 : 1});
 return 1; /* Other keys are captured until selection ends. */
}
