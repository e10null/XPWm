#include "../shared/font-settings.h"
/* Native Wayland taskbar. Temporary original Luna-style Cairo artwork. */
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <string.h>
#include <gio/gdesktopappinfo.h>
#include "xp-style.h"
#include <gtk-layer-shell.h>
#include <wayland-client.h>
#include "foreign-client.h"
#include "../shared/luna.h"
#include "start-menu.h"
#include "run-dialog.h"
#include "session-dialog.h"
#include "../shared/sounds.h"
#include "notifications.h"
#include "tray.h"
#include "taskbar-settings.h"
#include "taskbar-groups.h"

typedef struct {
	struct zwlr_foreign_toplevel_handle_v1 *handle;
	char *title, *app_id, *app_name;
 GdkPixbuf *icon;
 unsigned refs;
	gboolean active, minimized, maximized;
} Task;

static GtkWidget *bar, *canvas;
static struct wl_seat *seat;
static struct zwlr_foreign_toplevel_manager_v1 *manager;
static GList *tasks;
static Task *pressed, *hover, *menu_target;
static int first_task;
static GtkWidget *start_menu, *task_menu;
static GList *desktop_tasks;
static Task *desktop_focus;
static gboolean desktop_shown;
static gboolean start_pressed, start_hover;
static int quick_pressed, quick_hover;
static int tray_pressed = -1;
static gboolean tray_double;
static TaskbarSettings settings;
static GtkWidget *properties, *settings_checks[6];
static gboolean pointer_inside, bar_hidden;
static guint hide_timer;
static void schedule_hide(void);
static void set_hidden(gboolean hidden);
static void apply_settings(void);
static void open_properties(GtkMenuItem *item, gpointer data);
enum { BAR_HEIGHT = 30, START_WIDTH = 100, QUICK_WIDTH = 50, QUICK_BUTTON = 23 };

static void redraw(void) { if (canvas) gtk_widget_queue_draw(canvas); }
static Task *task_ref(Task *t) { t->refs++; return t; }
static void task_unref(gpointer data) {
 Task *t = data; if (--t->refs) return;
 g_clear_object(&t->icon); g_free(t->title); g_free(t->app_id); g_free(t->app_name); g_free(t);
}
static void show_desktop(void)
{
 if (desktop_shown) {
  for (GList *l = desktop_tasks; l; l = l->next) {
   Task *t = l->data;
   if (t->handle) zwlr_foreign_toplevel_handle_v1_unset_minimized(t->handle);
  }
  if (desktop_focus && desktop_focus->handle && seat)
   zwlr_foreign_toplevel_handle_v1_activate(desktop_focus->handle, seat);
  g_list_free_full(desktop_tasks, task_unref); desktop_tasks = NULL;
  if (desktop_focus) task_unref(desktop_focus);
 desktop_focus = NULL;
  desktop_shown = FALSE;
 } else {
  for (GList *l = tasks; l; l = l->next) {
   Task *t = l->data; if (t->minimized) continue;
   desktop_tasks = g_list_append(desktop_tasks, task_ref(t));
   if (t->active && !desktop_focus) desktop_focus = task_ref(t);
   zwlr_foreign_toplevel_handle_v1_set_minimized(t->handle);
  }
  desktop_shown = desktop_tasks != NULL;
 }
 redraw();
}

static void title(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, const char *s)
{
	Task *t = data;
	g_free(t->title); t->title = g_strdup(s);
}
static void appid(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, const char *s) {
 Task *t = data; g_clear_object(&t->icon);
 g_free(t->app_id); t->app_id = g_strdup(s);
 g_clear_pointer(&t->app_name, g_free);
 if (!s || !*s) return;
 char *id = g_str_has_suffix(s, ".desktop") ? g_strdup(s) : g_strconcat(s, ".desktop", NULL);
 GDesktopAppInfo *app = g_desktop_app_info_new(id); g_free(id);
 if (!app) return;
 t->app_name = g_strdup(g_app_info_get_display_name(G_APP_INFO(app)));
 GIcon *icon = g_app_info_get_icon(G_APP_INFO(app));
 GtkIconInfo *info = icon ? gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_default(), icon, 16, GTK_ICON_LOOKUP_FORCE_SIZE) : NULL;
 if (info) { t->icon = gtk_icon_info_load_icon(info, NULL); g_object_unref(info); }
 g_object_unref(app);
}
static void output(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, struct wl_output *o) { }
static void parent(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, struct zwlr_foreign_toplevel_handle_v1 *p) { }
static void state(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, struct wl_array *array)
{
	Task *t = data;
	uint32_t *s;
	t->active = t->minimized = t->maximized = FALSE;
	wl_array_for_each(s, array) {
		if (*s == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED) t->active = TRUE;
		if (*s == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED) t->minimized = TRUE;
		if (*s == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED) t->maximized = TRUE;
	}
}
static void done(void *data, struct zwlr_foreign_toplevel_handle_v1 *h) { redraw(); }

static void closed(void *data, struct zwlr_foreign_toplevel_handle_v1 *h)
{
	Task *t = data;
	if (pressed == t) pressed = NULL;
	if (hover == t) hover = NULL;
 if (task_menu) gtk_menu_popdown(GTK_MENU(task_menu));
	tasks = g_list_remove(tasks, t);
	zwlr_foreign_toplevel_handle_v1_destroy(h);
	t->handle = NULL; task_unref(t); redraw();
}
static const struct zwlr_foreign_toplevel_handle_v1_listener task_listener = {
	.title = title, .app_id = appid, .output_enter = output, .output_leave = output,
	.state = state, .done = done, .closed = closed, .parent = parent,
};
static void toplevel(void *data, struct zwlr_foreign_toplevel_manager_v1 *m,
		struct zwlr_foreign_toplevel_handle_v1 *h)
{
	Task *t = g_new0(Task, 1);
	t->handle = h; t->refs = 1; t->title = g_strdup("Untitled");
	tasks = g_list_append(tasks, t);
	zwlr_foreign_toplevel_handle_v1_add_listener(h, &task_listener, t);
}
static void finished(void *data, struct zwlr_foreign_toplevel_manager_v1 *m) { gtk_main_quit(); }
static const struct zwlr_foreign_toplevel_manager_v1_listener manager_listener = {
	.toplevel = toplevel, .finished = finished,
};
static void global(void *data, struct wl_registry *registry, uint32_t name,
		const char *interface, uint32_t version)
{
	if (!strcmp(interface, "zwlr_foreign_toplevel_manager_v1")) {
		manager = wl_registry_bind(registry, name, &zwlr_foreign_toplevel_manager_v1_interface, MIN(version, 3));
		zwlr_foreign_toplevel_manager_v1_add_listener(manager, &manager_listener, NULL);
	} else if (!strcmp(interface, "wl_seat") && !seat)
		seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
}
static void removed(void *data, struct wl_registry *r, uint32_t name) { }
static const struct wl_registry_listener registry_listener = {global, removed};

static void text(cairo_t *cr, const char *s, int x, int y, int width, const char *font)
{
	PangoLayout *layout = pango_cairo_create_layout(cr);
	pango_cairo_context_set_resolution(pango_layout_get_context(layout), xp_font_dpi());
	PangoFontDescription *desc = pango_font_description_from_string(font);
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_text(layout, s, -1);
	pango_layout_set_single_paragraph_mode(layout, TRUE);
	pango_layout_set_width(layout, MAX(1, width) * PANGO_SCALE);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	cairo_move_to(cr, x, y); pango_cairo_show_layout(cr, layout);
	pango_font_description_free(desc); g_object_unref(layout);
}
/* Luna's Start bitmap is a background strip; Explorer composites the four-pane
 * Windows mark and italic label over it. Keep the logo tied to the real button. */
static void start_logo(cairo_t *cr, int x, int y)
{
	if (xp_start_flag(cr, x, y)) return;
	cairo_save(cr); cairo_translate(cr, x, y); cairo_set_line_width(cr, .5);
	cairo_set_source_rgb(cr, .88, .12, .10); cairo_move_to(cr, 0, 1); cairo_line_to(cr, 7, 0); cairo_line_to(cr, 7, 7); cairo_line_to(cr, 0, 8); cairo_close_path(cr); cairo_fill(cr);
	cairo_set_source_rgb(cr, .15, .70, .18); cairo_move_to(cr, 8, 0); cairo_line_to(cr, 15, 0); cairo_line_to(cr, 15, 6); cairo_line_to(cr, 8, 7); cairo_close_path(cr); cairo_fill(cr);
	cairo_set_source_rgb(cr, .10, .35, .85); cairo_move_to(cr, 0, 9); cairo_line_to(cr, 7, 8); cairo_line_to(cr, 7, 15); cairo_line_to(cr, 0, 14); cairo_close_path(cr); cairo_fill(cr);
	cairo_set_source_rgb(cr, .96, .72, .08); cairo_move_to(cr, 8, 8); cairo_line_to(cr, 15, 7); cairo_line_to(cr, 15, 14); cairo_line_to(cr, 8, 15); cairo_close_path(cr); cairo_fill(cr);
	cairo_restore(cr);
}
static int quick_width(void) { return settings.quick_launch ? QUICK_WIDTH : 0; }
static const char *task_app_id(gpointer data) { return ((Task *)data)->app_id; }
static GPtrArray *task_buttons(void)
{
 int available = gtk_widget_get_allocated_width(canvas) - START_WIDTH - quick_width() - xp_tray_width() - 8;
 return taskbar_groups_build(tasks, task_app_id, settings.group_similar, available);
}
static int task_width(void)
{
 int available = gtk_widget_get_allocated_width(canvas) - START_WIDTH - quick_width() - xp_tray_width() - 8;
 GPtrArray *buttons = task_buttons();
 int width = CLAMP(available / MAX(1, (int)buttons->len), 70, 160);
 g_ptr_array_unref(buttons); return width;
}
static GPtrArray *task_members(Task *task)
{
 GPtrArray *buttons = task_buttons(), *members = g_ptr_array_new_with_free_func(task_unref);
 for (guint i = 0; i < buttons->len; i++) {
  GPtrArray *group = g_ptr_array_index(buttons, i);
  if (g_ptr_array_index(group, 0) != task) continue;
  for (guint j = 0; j < group->len; j++) g_ptr_array_add(members, task_ref(g_ptr_array_index(group, j)));
  break;
 }
 g_ptr_array_unref(buttons); return members;
}
static int quick_at(double x)
{
	int offset = (int)x - START_WIDTH - 4;
	if (offset < 0 || offset >= quick_width()) return 0;
	if (offset < QUICK_BUTTON) return 1;
	if (offset >= QUICK_BUTTON + 2 && offset < 2 * QUICK_BUTTON + 2) return 2;
	return 0;
}
static Task *task_at(double x)
{
	int width = gtk_widget_get_allocated_width(canvas);
	if (x < START_WIDTH + 4 + quick_width() || x >= width - xp_tray_width() - 4) return NULL;
	GPtrArray *buttons = task_buttons();
 int index = first_task + (int)(x - START_WIDTH - 4 - quick_width()) / task_width();
 Task *task = index >= 0 && (guint)index < buttons->len ? g_ptr_array_index((GPtrArray *)g_ptr_array_index(buttons, index), 0) : NULL;
 g_ptr_array_unref(buttons); return task;
}
static void quick_icon(cairo_t *cr, int which, int x, int y)
{
	if (xp_icon(cr, which == 1 ? 242 : 235, 16, x + 3, y + 1)) return;
	cairo_save(cr); cairo_set_line_width(cr, 1);
	if (which == 1) {
		cairo_set_source_rgb(cr, .78, .82, .82); cairo_rectangle(cr, x + 3, y + 2, 17, 13); cairo_fill_preserve(cr);
		cairo_set_source_rgb(cr, .15, .25, .35); cairo_stroke(cr); cairo_set_source_rgb(cr, .18, .43, .72);
		cairo_rectangle(cr, x + 5, y + 4, 13, 9); cairo_fill(cr); cairo_rectangle(cr, x + 9, y + 15, 5, 2); cairo_fill(cr);
	} else {
		cairo_set_source_rgb(cr, .97, .77, .25); cairo_move_to(cr, x + 2, y + 8); cairo_line_to(cr, x + 9, y + 8);
		cairo_line_to(cr, x + 12, y + 5); cairo_line_to(cr, x + 20, y + 5); cairo_line_to(cr, x + 22, y + 9);
		cairo_line_to(cr, x + 22, y + 18); cairo_line_to(cr, x + 2, y + 18); cairo_close_path(cr); cairo_fill_preserve(cr);
		cairo_set_source_rgb(cr, .45, .30, .04); cairo_stroke(cr);
	}
	cairo_restore(cr);
}
static gboolean draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	int width = gtk_widget_get_allocated_width(widget), x, i = 0, tw = task_width();
	GDateTime *now = g_date_time_new_now_local();
	char *clock = g_date_time_format(now, "%I:%M %p");
	/* XP's en-US short time has no leading zero (1:05 PM, not 01:05 PM). */
	if (clock && clock[0] == '0') memmove(clock, clock + 1, strlen(clock));
	cairo_pattern_t *gradient = cairo_pattern_create_linear(0, 0, 0, BAR_HEIGHT);
	cairo_pattern_add_color_stop_rgb(gradient, 0, .25, .56, .98);
	cairo_pattern_add_color_stop_rgb(gradient, .15, .08, .33, .90);
	cairo_pattern_add_color_stop_rgb(gradient, 1, .05, .25, .77);
	cairo_set_source(cr, gradient); cairo_paint(cr); cairo_pattern_destroy(gradient);
	luna_draw(cr, "taskbar", 0, 0, 0, width, BAR_HEIGHT);
	/* XP's Start bitmap is 33px high: its bottom three padding rows extend
	 * past the 30px taskbar. Preserve native height and clip that padding. */
 cairo_save(cr);cairo_rectangle(cr,0,0,START_WIDTH,BAR_HEIGHT);cairo_clip(cr);
	if (!luna_draw(cr, "start", start_menu || (start_pressed && start_hover) ? 2 : start_hover ? 1 : 0, 0, 0, START_WIDTH, BAR_HEIGHT+3)) {
		cairo_set_source_rgb(cr, .15, .53, .15); cairo_rectangle(cr, 0, 1, START_WIDTH, BAR_HEIGHT-1); cairo_fill(cr);
	}
 cairo_restore(cr);
	/* NORMALBLUE_INI: content 10,24,2,4; one shared optical center. */
	start_logo(cr, 10, 4);
	{
		PangoLayout *label = pango_cairo_create_layout(cr);
		PangoFontDescription *font = pango_font_description_from_string("Franklin Gothic Medium Bold Italic 14");
		PangoRectangle ink;
		pango_cairo_context_set_resolution(pango_layout_get_context(label), 96);
		pango_layout_set_font_description(label, font); pango_layout_set_text(label, "start", -1);
		pango_layout_get_pixel_extents(label, &ink, NULL);
		int y = 14-ink.y-ink.height/2;
		cairo_set_source_rgb(cr, 69/255., 76/255., 16/255.);
		cairo_move_to(cr, 39, y+2); pango_cairo_show_layout(cr, label);
		cairo_set_source_rgb(cr, 1, 1, 1);
		cairo_move_to(cr, 37, y); pango_cairo_show_layout(cr, label);
		pango_font_description_free(font); g_object_unref(label);
	}
	for (int q = 1; settings.quick_launch && q <= 2; q++) {
		int qx = START_WIDTH + 5 + (q - 1) * (QUICK_BUTTON + 2);
		if (quick_hover == q && !luna_draw(cr, "task-button", quick_pressed == q ? 2 : 1,
			qx, 3, QUICK_BUTTON, BAR_HEIGHT - 6)) {
			cairo_set_source_rgba(cr, .08, .35, .80, .35); cairo_rectangle(cr, qx, 3, QUICK_BUTTON, BAR_HEIGHT - 6); cairo_fill(cr);
		}
		quick_icon(cr, q, qx, 6);
	}
	if (settings.quick_launch) { cairo_set_source_rgba(cr, .04, .18, .50, .8); cairo_rectangle(cr, START_WIDTH + quick_width() + 3, 5, 1, BAR_HEIGHT - 10); cairo_fill(cr); }
	cairo_save(cr);
	cairo_rectangle(cr, START_WIDTH+4+quick_width(), 0, MAX(1, width-START_WIDTH-quick_width()-xp_tray_width()-8), BAR_HEIGHT); cairo_clip(cr);
	GPtrArray *buttons = task_buttons();
 first_task = CLAMP(first_task, 0, MAX(0, (int)buttons->len-1));
 for (guint n = first_task; n < buttons->len; n++, i++) {
  GPtrArray *members = g_ptr_array_index(buttons, n);
  Task *t = g_ptr_array_index(members, 0);
  gboolean active = FALSE;
  for (guint j = 0; j < members->len; j++) if (((Task *)g_ptr_array_index(members, j))->active) active = TRUE;
		x = START_WIDTH + 4 + quick_width() + i * tw;
		if (x >= width - xp_tray_width()) break;
		if (!luna_draw(cr, "task-button", t == menu_target || (t == pressed && t == hover) ? 2 :
				active ? (t == hover ? 5 : 4) : t == hover ? 1 : 0,
				x, 3, tw-3, BAR_HEIGHT-6)) {
			if (active || t == pressed || t == menu_target) cairo_set_source_rgb(cr, .03, .20, .60);
			else if (t == hover) cairo_set_source_rgb(cr, .30, .55, .96);
			else cairo_set_source_rgb(cr, .20, .44, .87);
			cairo_rectangle(cr, x+.5, 3.5, tw-3, BAR_HEIGHT-7); cairo_fill_preserve(cr);
			cairo_set_source_rgba(cr, .65, .80, 1, .8); cairo_set_line_width(cr, 1); cairo_stroke(cr);
		}
		if (t->icon) { gdk_cairo_set_source_pixbuf(cr, t->icon, x+5, 7); cairo_paint(cr); }
  else if (!xp_icon(cr, 3, 16, x+5, 7)) {
   cairo_set_source_rgb(cr, .95, .95, .9); cairo_rectangle(cr, x+6, 9, 12, 12); cairo_fill(cr);
   cairo_set_source_rgb(cr, .15, .30, .7); cairo_rectangle(cr, x+7, 10, 10, 3); cairo_fill(cr);
  }
		cairo_set_source_rgb(cr, 1, 1, 1);
  char *label = members->len > 1 ? g_strdup_printf("%u %s", members->len, t->app_name ? t->app_name : t->app_id) : g_strdup(t->title);
  text(cr, label, x+23, 8, tw-(members->len > 1 ? 40 : 29), active ? "Tahoma Bold 8" : "Tahoma 8"); g_free(label);
  if (members->len > 1) {
   cairo_move_to(cr, x+tw-13, 13); cairo_line_to(cr, x+tw-7, 13); cairo_line_to(cr, x+tw-10, 16); cairo_close_path(cr); cairo_fill(cr);
  }
	}
 g_ptr_array_unref(buttons);
	cairo_restore(cr);
	if (!luna_draw(cr, "tray", 0, width-xp_tray_width(), 0, xp_tray_width(), BAR_HEIGHT)) {
		cairo_set_source_rgb(cr, .04, .59, .88); cairo_rectangle(cr, width-xp_tray_width(), 1, xp_tray_width(), BAR_HEIGHT-1); cairo_fill(cr);
	}
	xp_tray_draw(cr, width-xp_tray_width(), BAR_HEIGHT);
	if (settings.clock) {
		PangoLayout *layout = pango_cairo_create_layout(cr);
		PangoFontDescription *font = pango_font_description_from_string("Tahoma 8");
		int cw, ch, clock_x = width-xp_tray_width()+xp_tray_clock_x();
		pango_cairo_context_set_resolution(pango_layout_get_context(layout), xp_font_dpi());
		pango_layout_set_font_description(layout, font);
		pango_layout_set_text(layout, clock, -1);
		pango_layout_get_pixel_size(layout, &cw, &ch);
		cairo_set_source_rgb(cr, 1, 1, 1);
		cairo_move_to(cr, clock_x+(xp_tray_clock_width()-cw)/2, (BAR_HEIGHT-ch)/2);
		pango_cairo_show_layout(cr, layout);
		pango_font_description_free(font); g_object_unref(layout);
	}
	g_free(clock); g_date_time_unref(now); return FALSE;
}
static gboolean dispose_menu(gpointer data)
{
	gtk_widget_destroy(GTK_WIDGET(data));
	g_object_unref(data);
	return G_SOURCE_REMOVE;
}

static void menu_keyboard(gboolean enabled)
{
	gtk_layer_set_keyboard_mode(GTK_WINDOW(bar), enabled ?
		GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE : GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
	/* Commit the layer state before the popup's grab request. Otherwise the
	 * grab preserves the application's old keyboard focus until dismissal. */
	wl_surface_commit(gdk_wayland_window_get_wl_surface(gtk_widget_get_window(bar)));
}

static void menu_closed(GtkWidget *menu, gpointer data)
{
	if (start_menu != menu) return;
	start_menu = NULL;
	start_pressed = start_hover = FALSE;
	menu_keyboard(FALSE); schedule_hide();
	/* Deactivation precedes item activation. Keep items alive until the current
	 * event has finished, including their application/folder callback data. */
	g_idle_add(dispose_menu, g_object_ref(menu));
	redraw();
}

static gboolean start_key_event(GtkWidget *widget, GdkEventKey *event, gpointer data);
static void toggle_start(GdkEvent *event)
{
	if (start_menu) { gtk_menu_popdown(GTK_MENU(start_menu)); return; }
	if (task_menu) gtk_menu_popdown(GTK_MENU(task_menu));
	set_hidden(FALSE);
	GdkRectangle anchor = {0, 0, START_WIDTH, BAR_HEIGHT};
	start_menu = start_menu_new();
	menu_keyboard(TRUE);
	gtk_menu_attach_to_widget(GTK_MENU(start_menu), canvas, NULL);
	g_signal_connect(start_menu, "selection-done", G_CALLBACK(menu_closed), NULL);
	g_signal_connect(start_menu, "hide", G_CALLBACK(menu_closed), NULL);
	g_signal_connect(start_menu, "key-press-event", G_CALLBACK(start_key_event), NULL);
	g_signal_connect(start_menu, "key-release-event", G_CALLBACK(start_key_event), NULL);
	gtk_menu_popup_at_rect(GTK_MENU(start_menu), gtk_widget_get_window(canvas),
		&anchor, GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_SOUTH_WEST, event);
	redraw();
}

static gboolean start_key_event(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	(void)widget; (void)data;
	if (event->keyval != GDK_KEY_Super_L && event->keyval != GDK_KEY_Super_R) return FALSE;
	if (event->type == GDK_KEY_RELEASE) {
		toggle_start((GdkEvent *)event);
		if (g_getenv("XPWM_START_KEY_TEST")) {
			char *path = g_build_filename(g_getenv("XDG_RUNTIME_DIR"), "start-key-state", NULL);
			g_file_set_contents(path, start_menu ? "open" : "closed", -1, NULL); g_free(path);
		}
	}
	return TRUE;
}

static void task_action(GtkMenuItem *item, gpointer data)
{
 Task *t = g_object_get_data(G_OBJECT(item), "target");
 if (!t || !t->handle) return;
 switch (GPOINTER_TO_INT(data)) {
 case 0:
  if (t->maximized) zwlr_foreign_toplevel_handle_v1_unset_maximized(t->handle);
  if (t->minimized) zwlr_foreign_toplevel_handle_v1_unset_minimized(t->handle);
  if (seat) zwlr_foreign_toplevel_handle_v1_activate(t->handle, seat);
  break;
 case 1: zwlr_foreign_toplevel_handle_v1_set_minimized(t->handle); break;
 case 2: zwlr_foreign_toplevel_handle_v1_set_maximized(t->handle); break;
 case 3: zwlr_foreign_toplevel_handle_v1_close(t->handle); break;
 }
}
static void task_show_desktop(GtkMenuItem *item, gpointer data) { show_desktop(); }
static void task_menu_closed(GtkWidget *menu, gpointer data)
{
 if (task_menu != menu) return;
 task_menu = NULL; g_clear_pointer(&menu_target, task_unref); menu_keyboard(FALSE); schedule_hide(); redraw();
 g_idle_add(dispose_menu, g_object_ref(menu));
}
static GtkWidget *task_menu_new(Task *t)
{
 GtkWidget *menu = gtk_menu_new();
 if (t) {
  const char *labels[] = {"Restore", "Minimize", "Maximize", "Close"};
  for (guint i = 0; i < G_N_ELEMENTS(labels); i++) {
   if (i == 3) gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
   GtkWidget *item = gtk_menu_item_new_with_label(labels[i]);
   g_object_set_data_full(G_OBJECT(item), "target", task_ref(t), task_unref);
   g_signal_connect(item, "activate", G_CALLBACK(task_action), GINT_TO_POINTER(i));
   gtk_widget_set_sensitive(item, i == 0 ? t->minimized || t->maximized : i == 1 ? !t->minimized : i == 2 ? !t->maximized : TRUE);
   gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  }
 } else {
  GtkWidget *item = gtk_menu_item_new_with_label(desktop_shown ? "Show Open Windows" : "Show the Desktop");
  gtk_widget_set_sensitive(item, desktop_shown || tasks != NULL);
  g_signal_connect(item, "activate", G_CALLBACK(task_show_desktop), NULL); gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
  item = gtk_menu_item_new_with_label("Properties");
  g_signal_connect(item, "activate", G_CALLBACK(open_properties), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
 }
 xp_style_apply(menu); gtk_widget_show_all(menu); return menu;
}

static void group_activate(GtkMenuItem *item, gpointer data)
{
 Task *task = g_object_get_data(G_OBJECT(item), "target");
 if (!task || !task->handle) return;
 if (task->minimized) zwlr_foreign_toplevel_handle_v1_unset_minimized(task->handle);
 if (seat) zwlr_foreign_toplevel_handle_v1_activate(task->handle, seat);
}
static void group_action(GtkMenuItem *item, gpointer data)
{
 GPtrArray *members = g_object_get_data(G_OBJECT(item), "members");
 for (guint i = 0; i < members->len; i++) {
  Task *task = g_ptr_array_index(members, i);
  if (!task->handle) continue;
  switch (GPOINTER_TO_INT(data)) {
  case 0:
   if (task->maximized) zwlr_foreign_toplevel_handle_v1_unset_maximized(task->handle);
   if (task->minimized) zwlr_foreign_toplevel_handle_v1_unset_minimized(task->handle);
   break;
  case 1: zwlr_foreign_toplevel_handle_v1_set_minimized(task->handle); break;
  case 2: zwlr_foreign_toplevel_handle_v1_close(task->handle); break;
  }
 }
}
static gboolean group_icon_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
 if (!xp_icon(cr, 3, 16, 0, 0)) {
  cairo_set_source_rgb(cr, .95, .95, .9); cairo_rectangle(cr, 1, 1, 14, 14); cairo_fill(cr);
  cairo_set_source_rgb(cr, .15, .30, .7); cairo_rectangle(cr, 2, 2, 12, 3); cairo_fill(cr);
 }
 return FALSE;
}
static GtkWidget *group_menu_new(GPtrArray *members, gboolean context)
{
 GtkWidget *menu = gtk_menu_new();
 if (context) {
  const char *labels[] = {"Restore Group", "Minimize Group", "Close Group"};
  gboolean restorable = FALSE, minimizable = FALSE;
  for (guint i = 0; i < members->len; i++) {
   Task *task = g_ptr_array_index(members, i);
   restorable |= task->maximized || task->minimized; minimizable |= !task->minimized;
  }
  for (guint i = 0; i < G_N_ELEMENTS(labels); i++) {
   if (i == 2) gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
   GtkWidget *item = gtk_menu_item_new_with_label(labels[i]);
   g_object_set_data_full(G_OBJECT(item), "members", g_ptr_array_ref(members), (GDestroyNotify)g_ptr_array_unref);
   g_signal_connect(item, "activate", G_CALLBACK(group_action), GINT_TO_POINTER(i));
   gtk_widget_set_sensitive(item, i == 0 ? restorable : i == 1 ? minimizable : TRUE);
   gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  }
 } else {
  for (guint i = 0; i < members->len; i++) {
   Task *task = g_ptr_array_index(members, i);
   GtkWidget *item = gtk_menu_item_new();
   GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
   GtkWidget *icon = task->icon ? gtk_image_new_from_pixbuf(task->icon) : gtk_drawing_area_new();
   if (!task->icon) { gtk_widget_set_size_request(icon, 16, 16); g_signal_connect(icon, "draw", G_CALLBACK(group_icon_draw), NULL); }
   GtkWidget *label = gtk_label_new(task->title);
   gtk_label_set_xalign(GTK_LABEL(label), 0); gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
   gtk_label_set_max_width_chars(GTK_LABEL(label), 60);
   if (task->active) {
    PangoAttrList *attrs = pango_attr_list_new(); pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes(GTK_LABEL(label), attrs); pango_attr_list_unref(attrs);
   }
   gtk_box_pack_start(GTK_BOX(row), icon, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(row), label, TRUE, TRUE, 0);
   gtk_container_add(GTK_CONTAINER(item), row);
   g_object_set_data_full(G_OBJECT(item), "target", task_ref(task), task_unref);
   g_signal_connect(item, "activate", G_CALLBACK(group_activate), NULL);
   gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  }
 }
 xp_style_apply(menu); gtk_widget_show_all(menu); return menu;
}
static void task_popup(GtkWidget *menu, GdkEventButton *event, Task *target)
{
 task_menu = menu; menu_target = target ? task_ref(target) : NULL; menu_keyboard(TRUE);
 gtk_menu_attach_to_widget(GTK_MENU(menu), canvas, NULL);
 g_signal_connect(menu, "selection-done", G_CALLBACK(task_menu_closed), NULL);
 g_signal_connect(menu, "hide", G_CALLBACK(task_menu_closed), NULL);
 GdkRectangle anchor = {(int)event->x, 0, 1, BAR_HEIGHT};
 gtk_menu_popup_at_rect(GTK_MENU(menu), gtk_widget_get_window(canvas), &anchor,
  GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_SOUTH_WEST, (GdkEvent *)event);
}
static gboolean button(GtkWidget *w, GdkEventButton *event, gpointer data)
{
	Task *t = task_at(event->x);
	int quick = quick_at(event->x);
	int tray_x = event->x - (gtk_widget_get_allocated_width(canvas)-xp_tray_width());
	if (tray_x >= 0) {
		if (event->type == GDK_BUTTON_PRESS) { tray_pressed = xp_tray_hit(tray_x); tray_double = FALSE; }
  if (event->type == GDK_2BUTTON_PRESS && event->button == 1) { tray_double = TRUE; xp_tray_double_click(tray_x); }
		if (event->type == GDK_BUTTON_RELEASE) {
			if (!tray_double && tray_pressed >= 0 && tray_pressed == xp_tray_hit(tray_x)) xp_tray_click(tray_x, event->button);
			tray_pressed = -1;
		}
		pressed = NULL; start_pressed = FALSE; quick_pressed = 0;
		redraw(); return TRUE;
	}
	tray_pressed = -1;
 if (event->button == 3 && event->type == GDK_BUTTON_PRESS && !task_menu && !start_menu) {
  GPtrArray *members = task_members(t);
  task_popup(members->len > 1 ? group_menu_new(members, TRUE) : task_menu_new(t), event, t);
  g_ptr_array_unref(members);
  return TRUE;
 }
	if (event->button != 1) return FALSE;
	if (event->type == GDK_BUTTON_PRESS) {
		pressed = t;
		quick_pressed = quick;
		start_pressed = event->x >= 0 && event->x < START_WIDTH;
		start_hover = start_pressed;
	}
	else if (event->type == GDK_BUTTON_RELEASE) {
		if (start_pressed && event->x >= 0 && event->x < START_WIDTH &&
				event->y >= 0 && event->y < BAR_HEIGHT && !start_menu) {
			toggle_start((GdkEvent *)event);
		}
		if (quick_pressed && quick_pressed == quick_hover) {
			if (quick_pressed == 1) {
				show_desktop();
			} else {
				GError *error = NULL;
				char *uri = g_filename_to_uri(g_get_home_dir(), NULL, &error);
				if (uri) gtk_show_uri_on_window(GTK_WINDOW(bar), uri, event->time, &error);
				g_free(uri); if (error) g_clear_error(&error);
			}
		}
		if (t && pressed == t && seat && !task_menu && !start_menu) {
   GPtrArray *members = task_members(t);
   if (members->len > 1) task_popup(group_menu_new(members, FALSE), event, t);
   else if (t->active && !t->minimized) zwlr_foreign_toplevel_handle_v1_set_minimized(t->handle);
			else zwlr_foreign_toplevel_handle_v1_activate(t->handle, seat);
   g_ptr_array_unref(members);
		}
		pressed = NULL;
		start_pressed = FALSE;
		quick_pressed = 0;
	}
	redraw(); return TRUE;
}
static void set_hidden(gboolean hidden)
{
 bar_hidden = hidden && settings.auto_hide;
 gtk_layer_set_margin(GTK_WINDOW(bar), GTK_LAYER_SHELL_EDGE_BOTTOM, bar_hidden ? 2-BAR_HEIGHT : 0);
 gtk_layer_set_exclusive_zone(GTK_WINDOW(bar), settings.auto_hide ? (bar_hidden ? BAR_HEIGHT : 2) : BAR_HEIGHT);
 gtk_widget_queue_draw(canvas);
}
static gboolean hide_later(gpointer data)
{
 hide_timer = 0;
 if (settings.auto_hide && !pointer_inside && !start_menu && !task_menu && !properties && !xp_tray_popup_open())
  set_hidden(TRUE);
 else if (settings.auto_hide && !pointer_inside) schedule_hide();
 return G_SOURCE_REMOVE;
}
static void schedule_hide(void)
{
 if (hide_timer) g_source_remove(hide_timer);
 hide_timer = 0;
 if (settings.auto_hide && !pointer_inside) hide_timer = g_timeout_add(600, hide_later, NULL);
}
static void apply_settings(void)
{
 xp_tray_set_clock_visible(settings.clock);
 first_task = 0; pressed = hover = NULL; quick_pressed = quick_hover = 0;
 gtk_layer_set_layer(GTK_WINDOW(bar), settings.on_top ? GTK_LAYER_SHELL_LAYER_TOP : GTK_LAYER_SHELL_LAYER_BOTTOM);
 set_hidden(FALSE); schedule_hide(); redraw();
}
static void properties_destroyed(GtkWidget *widget, gpointer data) { properties = NULL; schedule_hide(); }
static void properties_response(GtkDialog *dialog, int response, gpointer data)
{
 if (response == GTK_RESPONSE_APPLY || response == GTK_RESPONSE_OK) {
  TaskbarSettings draft = {
   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(settings_checks[0])),
   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(settings_checks[1])),
   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(settings_checks[2])),
   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(settings_checks[3])),
   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(settings_checks[4])),
   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(settings_checks[5]))};
  GError *error = NULL;
  if (!taskbar_settings_save(&draft, &error)) {
   GtkWidget *message = gtk_message_dialog_new(GTK_WINDOW(dialog), GTK_DIALOG_DESTROY_WITH_PARENT,
    GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Unable to save taskbar settings");
   gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(message), "%s", error->message);
   xp_style_apply(message); g_signal_connect_swapped(message, "response", G_CALLBACK(gtk_widget_destroy), message);
   gtk_widget_show(message); g_error_free(error); return;
  }
  settings = draft; apply_settings();
 }
 if (response != GTK_RESPONSE_APPLY) gtk_widget_destroy(GTK_WIDGET(dialog));
}
static void open_properties(GtkMenuItem *item, gpointer data)
{
 if (properties) { gtk_window_present(GTK_WINDOW(properties)); return; }
 set_hidden(FALSE);
 properties = gtk_dialog_new_with_buttons("Taskbar Properties", NULL, 0,
  "OK", GTK_RESPONSE_OK, "Cancel", GTK_RESPONSE_CANCEL, "Apply", GTK_RESPONSE_APPLY, NULL);
 gtk_window_set_resizable(GTK_WINDOW(properties), FALSE);
 GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(properties));
 gtk_container_set_border_width(GTK_CONTAINER(box), 14); gtk_box_set_spacing(GTK_BOX(box), 12);
 GtkWidget *title = gtk_label_new("Taskbar appearance"); gtk_widget_set_halign(title, GTK_ALIGN_START);
 gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);
 const char *labels[] = {"Show the _clock", "Show _Quick Launch", "_Auto-hide the taskbar", "_Keep the taskbar on top of other windows", "_Group similar taskbar buttons", "Activation follows _mouse (X-Mouse)"};
 const gboolean values[] = {settings.clock, settings.quick_launch, settings.auto_hide, settings.on_top, settings.group_similar, settings.focus_follows_mouse};
 for (guint i = 0; i < G_N_ELEMENTS(labels); i++) {
  if (i == 5) {
   GtkWidget *heading = gtk_label_new("Window behavior");
   gtk_widget_set_halign(heading, GTK_ALIGN_START);
   gtk_box_pack_start(GTK_BOX(box), heading, FALSE, FALSE, 0);
  }
  settings_checks[i] = gtk_check_button_new_with_mnemonic(labels[i]);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(settings_checks[i]), values[i]);
  gtk_box_pack_start(GTK_BOX(box), settings_checks[i], FALSE, FALSE, 0);
 }
 gtk_widget_set_tooltip_text(settings_checks[5], "Focus the window under the pointer without clicking or raising it.");
 g_signal_connect(properties, "response", G_CALLBACK(properties_response), NULL);
 g_signal_connect(properties, "destroy", G_CALLBACK(properties_destroyed), NULL);
 xp_style_apply(properties); gtk_widget_show_all(properties);
}
static gboolean enter(GtkWidget *w, GdkEventCrossing *event, gpointer data)
{
 pointer_inside = TRUE;
 if (hide_timer) g_source_remove(hide_timer);
 hide_timer = 0;
 if (bar_hidden) set_hidden(FALSE);
 return FALSE;
}
static gboolean motion(GtkWidget *w, GdkEventMotion *event, gpointer data)
{
	hover = task_at(event->x);
	quick_hover = quick_at(event->x);
	start_hover = event->x >= 0 && event->x < START_WIDTH;
	int tray_x = event->x - (gtk_widget_get_allocated_width(canvas)-xp_tray_width());
	GPtrArray *members = task_members(hover);
 char *group_tip = members->len > 1 ? g_strdup_printf("%s (%u windows)", hover->app_name ? hover->app_name : hover->app_id, members->len) : NULL;
 gtk_widget_set_tooltip_text(w, tray_x >= 0 ? xp_tray_tooltip(tray_x) : hover ? (group_tip ? group_tip : hover->title) : start_hover ? "Start" :
  quick_hover == 1 ? (desktop_shown ? "Show Open Windows" : "Show Desktop") : quick_hover == 2 ? "Home" : NULL);
 g_free(group_tip); g_ptr_array_unref(members);
	redraw(); return TRUE;
}
static gboolean leave(GtkWidget *w, GdkEventCrossing *event, gpointer data) { pointer_inside = FALSE; schedule_hide(); hover = NULL; quick_hover = 0; start_hover = FALSE; redraw(); return TRUE; }
static gboolean scroll(GtkWidget *w, GdkEventScroll *event, gpointer data)
{
	int tray_x = event->x - (gtk_widget_get_allocated_width(canvas)-xp_tray_width());
	if (tray_x >= 0) return xp_tray_scroll(tray_x, event->direction);
	first_task += event->direction == GDK_SCROLL_UP ? -1 : 1; redraw(); return TRUE;
}
#include "taskbar-group-test.h"

static gboolean tick(gpointer data) { redraw(); return G_SOURCE_CONTINUE; }

/* Optional protocol integration check, enabled only by the test runner. */
static void test_menu_action(Task *task, guint index)
{
 GtkWidget *menu = task_menu_new(task);
 GList *items = gtk_container_get_children(GTK_CONTAINER(menu));
 GtkWidget *item = g_list_nth_data(items, index);
 g_assert_true(gtk_widget_get_sensitive(item)); gtk_menu_item_activate(GTK_MENU_ITEM(item));
 g_list_free(items); gtk_widget_destroy(menu);
}
static gboolean test_tick(gpointer data)
{
 static int stage, tries;
 static Task *protected, *target;
 if (++tries > 100) g_error("Taskbar protocol test timed out at stage %d", stage);
 if (stage == 0) {
  if (g_list_length(tasks) < 2 || !seat) return G_SOURCE_CONTINUE;
  for (GList *l = tasks; l; l = l->next) { Task *t = l->data; if (t->active) target = t; }
  if (!target) return G_SOURCE_CONTINUE;
  protected = tasks->data == target ? tasks->next->data : tasks->data;
  GtkWidget *menu = start_menu_new(); GList *items = gtk_container_get_children(GTK_CONTAINER(menu));
  g_assert_cmpuint(g_list_length(items), >=, 18); g_list_free(items); gtk_widget_destroy(menu);
  zwlr_foreign_toplevel_handle_v1_set_minimized(protected->handle); stage++;
 } else if (stage == 1 && protected->minimized) { show_desktop(); stage++; }
 else if (stage == 2 && target->minimized) {
  g_assert_true(protected->minimized); show_desktop(); stage++;
 } else if (stage == 3 && !target->minimized && target->active) {
  g_assert_true(protected->minimized);
  g_print("PASS: Show Desktop restores only its own windows and preserves existing minimization\n");
  test_menu_action(target, 1); stage++;
 } else if (stage == 4 && target->minimized) { test_menu_action(target, 0); stage++; }
 else if (stage == 5 && !target->minimized && target->active) { test_menu_action(target, 2); stage++; }
 else if (stage == 6 && target->maximized) { test_menu_action(target, 0); stage++; }
 else if (stage == 7 && !target->maximized) { test_menu_action(target, 4); stage++; }
 else if (stage == 8 && g_list_length(tasks) == 1) {
  g_print("PASS: task context-menu minimize/restore/maximize/close use real compositor actions\n");
  test_menu_action(protected, 4); stage++;
 }
 return G_SOURCE_CONTINUE;
}

static gboolean test_done(gpointer data)
{
	static gboolean had_task;
	const char *marker = g_getenv("XPWM_TASKBAR_TEST");
	if (tasks) had_task = TRUE;
	if (had_task && !tasks) {
  GtkWidget *successful = run_dialog_open();
  GtkWidget *successful_input = gtk_window_get_focus(GTK_WINDOW(successful));
  gtk_entry_set_text(GTK_ENTRY(successful_input), "/bin/true");
  gtk_dialog_response(GTK_DIALOG(successful), GTK_RESPONSE_OK);
		GtkWidget *run = run_dialog_open();
  GtkWidget *history = g_object_get_data(G_OBJECT(run), "run-history-combo");
  g_assert_true(GTK_IS_COMBO_BOX(history));
  gtk_combo_box_set_active(GTK_COMBO_BOX(history), 0);
  g_assert_cmpstr(gtk_entry_get_text(GTK_ENTRY(gtk_window_get_focus(GTK_WINDOW(run)))), ==, "/bin/true");
  gtk_entry_set_text(GTK_ENTRY(gtk_window_get_focus(GTK_WINDOW(run))), "");
		GtkWidget *input = gtk_window_get_focus(GTK_WINDOW(run));
		g_assert(run_dialog_open() == run);
		g_assert(GTK_IS_ENTRY(input));
		g_assert_false(gtk_widget_get_sensitive(gtk_dialog_get_widget_for_response(GTK_DIALOG(run), GTK_RESPONSE_OK)));
		gtk_entry_set_text(GTK_ENTRY(input), "/nonexistent/xpwm-test-program");
		g_assert_true(gtk_widget_get_sensitive(gtk_dialog_get_widget_for_response(GTK_DIALOG(run), GTK_RESPONSE_OK)));
		gtk_dialog_response(GTK_DIALOG(run), GTK_RESPONSE_OK);
		g_assert_true(gtk_widget_get_visible(run));
		gtk_dialog_response(GTK_DIALOG(run), GTK_RESPONSE_CANCEL);
		run = run_dialog_open();
		gtk_dialog_response(GTK_DIALOG(run), GTK_RESPONSE_CANCEL);
		{
			GtkWidget *session = session_dialog_open();
			const char *session_marker = g_getenv("XPWM_SESSION_MARKER");
			char *action = NULL;
			g_assert_cmpstr(gtk_window_get_title(GTK_WINDOW(session)), ==, "Turn off computer");
			gtk_dialog_response(GTK_DIALOG(session), XPWM_SESSION_POWEROFF);
			g_assert(session_marker && g_file_get_contents(session_marker, &action, NULL, NULL));
			g_assert_cmpstr(action, ==, "poweroff");
			g_free(action);
		}
		{
			GtkWidget *logoff = logoff_dialog_open();
			const char *session_marker = g_getenv("XPWM_SESSION_MARKER");
			char *action = NULL;
			g_assert_cmpstr(gtk_window_get_title(GTK_WINDOW(logoff)), ==, "Log Off Windows");
			gtk_dialog_response(GTK_DIALOG(logoff), XPWM_SESSION_LOGOFF);
			g_assert(session_marker && g_file_get_contents(session_marker, &action, NULL, NULL));
			g_assert_cmpstr(action, ==, "logoff");
			g_free(action);
		}
		g_assert(g_file_set_contents(marker, "PASS: taskbar enumerate/minimize/activate/close\n", -1, NULL));
		g_print("PASS: Run dialog, logon/logoff session dialogs, dry-run actions, cancel/reopen\n");
		gtk_main_quit(); return G_SOURCE_REMOVE;
	}
	return G_SOURCE_CONTINUE;
}
static void settings_marker(const char *name)
{
 char *path = g_build_filename(g_getenv("XDG_RUNTIME_DIR"), name, NULL);
 g_assert_true(g_file_set_contents(path, "passed", -1, NULL)); g_free(path);
}
static gboolean settings_marked(const char *name)
{
 char *path = g_build_filename(g_getenv("XDG_RUNTIME_DIR"), name, NULL);
 gboolean exists = g_file_test(path, G_FILE_TEST_EXISTS); g_free(path); return exists;
}
static void settings_test_draft(gboolean defaults)
{
 const gboolean values[] = {defaults, defaults, !defaults, defaults, defaults, !defaults};
 for (guint i = 0; i < G_N_ELEMENTS(values); i++) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(settings_checks[i]), values[i]);
}
static gboolean settings_test(gpointer data)
{
 static int phase, attempts;
 if (++attempts > 90) g_error("Taskbar settings test timed out at phase %d", phase);
 if (!gtk_widget_get_mapped(bar)) return G_SOURCE_CONTINUE;
 switch (phase) {
 case 0: {
  g_assert_true(settings.clock && settings.quick_launch && settings.on_top && settings.group_similar && !settings.auto_hide);
  open_properties(NULL, NULL); GtkWidget *first = properties; open_properties(NULL, NULL); g_assert_true(first == properties);
  settings_test_draft(FALSE); gtk_dialog_response(GTK_DIALOG(properties), GTK_RESPONSE_CANCEL);
  g_assert_true(settings.clock && settings.quick_launch && !settings.auto_hide && settings.on_top);
  g_assert_false(settings.focus_follows_mouse);
  open_properties(NULL, NULL); settings_test_draft(FALSE);
  gtk_dialog_response(GTK_DIALOG(properties), GTK_RESPONSE_APPLY);
  g_assert_false(settings.clock); g_assert_false(settings.quick_launch); g_assert_false(settings.on_top);
  g_assert_true(settings.auto_hide);
  g_assert_true(settings.focus_follows_mouse);
  g_assert_cmpint(quick_width(), ==, 0); g_assert_cmpint(quick_at(START_WIDTH+10), ==, 0);
  g_assert_cmpint(xp_tray_clock_width(), ==, 0); g_assert_cmpint(xp_tray_hit(xp_tray_width()-1), ==, -1);
  TaskbarSettings loaded; taskbar_settings_load(&loaded);
  g_assert_true(loaded.focus_follows_mouse);
  g_assert_false(loaded.clock || loaded.quick_launch || loaded.on_top || loaded.group_similar); g_assert_true(loaded.auto_hide);
  settings_test_draft(TRUE); gtk_dialog_response(GTK_DIALOG(properties), GTK_RESPONSE_CANCEL);
  g_assert_false(settings.clock); pointer_inside = FALSE; schedule_hide(); phase++; break;
 }
 case 1:
  if (!bar_hidden) break;
  g_assert_cmpint(gtk_layer_get_margin(GTK_WINDOW(bar), GTK_LAYER_SHELL_EDGE_BOTTOM), ==, 2-BAR_HEIGHT);
  settings_marker("settings-hidden"); phase++; break;
 case 2:
  if (!settings_marked("settings-expanded-geometry") || bar_hidden) break;
  g_assert_true(pointer_inside);
  open_properties(NULL, NULL); settings_test_draft(TRUE);
  gtk_dialog_response(GTK_DIALOG(properties), GTK_RESPONSE_OK);
  g_assert_null(properties);
  g_assert_true(settings.clock && settings.quick_launch && settings.on_top && settings.group_similar && !settings.auto_hide);
  g_assert_false(settings.focus_follows_mouse);
  g_assert_cmpint(xp_tray_clock_width(), ==, 58); g_assert_cmpint(quick_width(), ==, QUICK_WIDTH);
  settings_marker("settings-restored"); phase++; break;
 case 3:
  if (!settings_marked("settings-normal-geometry")) break;
  settings_marker("settings-passed");
  g_print("PASS: taskbar Properties singleton, Cancel/Apply/OK, saved toggles, native auto-hide/reveal, Quick Launch and clock hit regions\n");
  gtk_main_quit(); return G_SOURCE_REMOVE;
 }
 return G_SOURCE_CONTINUE;
}
int main(int argc, char **argv)
{
	struct wl_display *display;
	struct wl_registry *registry;
	/* Custom-drawn chrome must not inherit third-party GTK theme metrics. */
	luna_init();
	gtk_init(&argc, &argv);
	taskbar_settings_load(&settings);
	if (!GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default()) || !gtk_layer_is_supported())
		g_error("XPWm taskbar requires Wayland and layer-shell");
	display = gdk_wayland_display_get_wl_display(gdk_display_get_default());
	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	if (wl_display_roundtrip(display) < 0 || !manager) g_error("Foreign-toplevel management unavailable");
	bar = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_layer_init_for_window(GTK_WINDOW(bar));
	gtk_layer_set_namespace(GTK_WINDOW(bar), "xpwm-taskbar");
	gtk_layer_set_layer(GTK_WINDOW(bar), GTK_LAYER_SHELL_LAYER_TOP);
	gtk_layer_set_keyboard_mode(GTK_WINDOW(bar), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
	gtk_layer_set_anchor(GTK_WINDOW(bar), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(bar), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(bar), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
	gtk_layer_set_exclusive_zone(GTK_WINDOW(bar), BAR_HEIGHT);
	canvas = gtk_drawing_area_new(); gtk_widget_set_size_request(canvas, 200, BAR_HEIGHT);
	gtk_container_add(GTK_CONTAINER(bar), canvas);
	gtk_widget_add_events(canvas, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_SCROLL_MASK);
	g_signal_connect(canvas, "draw", G_CALLBACK(draw), NULL);
	g_signal_connect(canvas, "button-press-event", G_CALLBACK(button), NULL);
	g_signal_connect(canvas, "button-release-event", G_CALLBACK(button), NULL);
	g_signal_connect(canvas, "motion-notify-event", G_CALLBACK(motion), NULL);
	g_signal_connect(canvas, "enter-notify-event", G_CALLBACK(enter), NULL);
	g_signal_connect(canvas, "leave-notify-event", G_CALLBACK(leave), NULL);
	g_signal_connect(canvas, "scroll-event", G_CALLBACK(scroll), NULL);
	g_signal_connect(bar, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(bar, "key-press-event", G_CALLBACK(start_key_event), NULL);
	g_signal_connect(bar, "key-release-event", G_CALLBACK(start_key_event), NULL);
	g_timeout_add_seconds(1, tick, NULL);
	xp_tray_init(canvas);
	gtk_widget_show_all(bar);
	apply_settings();
	xp_notifications_start();
	if (g_getenv("XPWM_TASKBAR_GROUP_TEST")) g_timeout_add(100, group_test, NULL);
	if (g_getenv("XPWM_TASKBAR_SETTINGS_TEST")) g_timeout_add(100, settings_test, NULL);
	if (g_getenv("XPWM_TASKBAR_TEST")) {
		g_timeout_add(100, test_tick, NULL); g_timeout_add(100, test_done, NULL);
	}
	gtk_main();
	if (hide_timer) g_source_remove(hide_timer);
 hide_timer = 0;
	if (properties) gtk_widget_destroy(properties);
 if (hide_timer) g_source_remove(hide_timer);
 hide_timer = 0;
	xp_notifications_stop();
	xp_tray_stop();
	if (start_menu) gtk_widget_destroy(start_menu);
 if (task_menu) gtk_widget_destroy(task_menu);
 g_clear_pointer(&menu_target, task_unref);
 g_list_free_full(desktop_tasks, task_unref); desktop_tasks = NULL;
 if (desktop_focus) task_unref(desktop_focus);
 desktop_focus = NULL;
	while (tasks) { Task *t = tasks->data; closed(t, t->handle); }
	if (seat) wl_seat_destroy(seat);
	zwlr_foreign_toplevel_manager_v1_destroy(manager);
	wl_registry_destroy(registry);
	luna_finish();
	return 0;
}
