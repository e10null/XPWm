/* Mixed real clients: mode changes, size hints, layers and configure states. */
#define main dwl_main
#include "../compositor/dwl.c"
#undef main
#include <assert.h>
static struct wl_event_source *timer;
static int phase, attempts, delay;
static struct wlr_box saved;
static Client *browser, *terminal, *hints, *dialog;
static struct wl_resource *configured[16];
static int configured_count, normal_initial;
static void first_configure(void *data, enum wl_protocol_logger_type direction,
 const struct wl_protocol_logger_message *message)
{
 struct wlr_xdg_toplevel *top;
 uint32_t *state;
 int edges = 0, tiled;
 (void)data;
 if (direction != WL_PROTOCOL_LOGGER_EVENT ||
  strcmp(wl_resource_get_class(message->resource), "xdg_toplevel") ||
  strcmp(message->message->name, "configure")) return;
 for (int i = 0; i < configured_count; i++) if (configured[i] == message->resource) return;
 assert(configured_count < 16);
 configured[configured_count++] = message->resource;
 top = wlr_xdg_toplevel_from_resource(message->resource);
 wl_array_for_each(state, message->arguments[2].a)
  if (*state >= XDG_TOPLEVEL_STATE_TILED_LEFT && *state <= XDG_TOPLEVEL_STATE_TILED_BOTTOM) edges++;
 tiled = getenv("XPWM_LAYOUT_START_TILED") && !top->parent;
 assert(edges == (tiled ? 4 : 0));
 if (!top->parent) normal_initial++;
}
static void initial_output(struct wl_listener *listener, void *data)
{
 (void)listener; (void)data;
 if (getenv("XPWM_LAYOUT_START_TILED")) setlayout(&(Arg){.v=&layouts[0]});
}
static void checktiles(void)
{
 Client *c, *other;
 struct wlr_box overlap;
 wl_list_for_each(c, &clients, link) {
  if (c->isfloating || c->isfullscreen) continue;
  assert(c->scene->node.parent == layers[LyrTile]);
  assert(c->geom.x >= c->mon->w.x && c->geom.y >= c->mon->w.y);
  assert(c->geom.x+c->geom.width <= c->mon->w.x+c->mon->w.width);
  assert(c->geom.y+c->geom.height <= c->mon->w.y+c->mon->w.height);
  assert(c->surface.xdg->toplevel->current.tiled ==
   (WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT));
  wl_list_for_each(other, &clients, link) {
   if (c == other || other->isfloating || other->isfullscreen) continue;
   assert(!wlr_box_intersection(&overlap, &c->geom, &other->geom));
  }
 }
 assert(dialog->isfloating);
 assert(dialog->surface.xdg->toplevel->current.tiled == 0);
}
static int step(void *data)
{
 Client *c;
 (void)data;
 assert(++attempts < 240);
 wl_list_for_each(c, &clients, link) {
  const char *title = client_get_title(c), *app = client_get_appid(c);
  if (title && !strcmp(title, "XPWm layout hints")) hints = c;
  if (title && !strcmp(title, "XPWm layout dialog")) dialog = c;
  if (app && !strcmp(app, "xpwm-layout-terminal")) terminal = c;
  if (app && strstr(app, "firefox")) browser = c;
 }
 if (!hints || !dialog || !terminal || (getenv("XPWM_LAYOUT_FIREFOX") && !browser)) goto again;
 if (delay++ < 5) goto again;
 wl_list_for_each(c, &clients, link) if (c->resize) goto again;
 switch (phase++) {
 case 0:
  assert(normal_initial >= 2);
  if (getenv("XPWM_LAYOUT_START_TILED")) { checktiles(); xpallfloat(&(Arg){0}); }
  /* Mimic a browser opening with its remembered maximized state. */
  xpmaximize(browser ? browser : terminal, 1);
  saved = (browser ? browser : terminal)->normalgeom;
  xpallfloat(&(Arg){0}); delay = 0; break;
 case 1:
  checktiles();
  assert(!(browser ? browser : terminal)->maximized);
  /* A client floated by a drag must return to the tile layer. */
  setfloating(terminal, 1);
  assert(terminal->scene->node.parent == layers[LyrFloat]);
  xpallfloat(&(Arg){0});
  assert((browser ? browser : terminal)->maximized);
  xpallfloat(&(Arg){0}); delay = 0; break;
 case 2:
  checktiles();
  if (getenv("XPWM_LAYOUT_SCREENSHOT")) spawn(&(Arg)SHCMD("grim \"$XPWM_LAYOUT_SCREENSHOT\""));
  delay = 0; break;
 case 3:
  xpallfloat(&(Arg){0}); delay = 0; break;
 case 4:
  c = browser ? browser : terminal;
  assert(c->maximized);
  xpmaximize(c, 0);
  assert(wlr_box_equal(&saved, &c->geom));
  delay = 0; break;
 case 5:
  wl_list_for_each(c, &clients, link) {
   assert(c->surface.xdg->toplevel->current.tiled == 0);
   if (!c->maximized) assert(c->scene->node.parent == layers[LyrTile]);
  }
  fprintf(stderr, "PASS: mixed GTK/terminal%s tiling, oversized hints, nonoverlap, layers, maximize restore, floating configure state\n", browser ? "/Firefox" : "");
  wl_display_terminate(dpy); return 0;
 }
again:
 wl_event_source_timer_update(timer, 100); return 0;
}
int main(void)
{
 struct wl_listener output_listener = {.notify=initial_output};
 struct wl_protocol_logger *logger;
 setup();
 logger = wl_display_add_protocol_logger(dpy, first_configure, NULL);
 wl_signal_add(&backend->events.new_output, &output_listener);
 timer = wl_event_loop_add_timer(event_loop, step, NULL);
 wl_event_source_timer_update(timer, 100);
 run("sh tests/layout-client.sh");
 assert(phase == 6);
 wl_event_source_remove(timer); wl_list_remove(&output_listener.link);
 wl_protocol_logger_destroy(logger); cleanup(); return 0;
}
