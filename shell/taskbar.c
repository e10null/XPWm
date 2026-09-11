/* Native Wayland taskbar. Temporary original Luna-style Cairo artwork. */
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <string.h>
#include <gtk-layer-shell.h>
#include <wayland-client.h>
#include "foreign-client.h"
#include "../shared/luna.h"
#include "start-menu.h"
#include "run-dialog.h"
#include "session-dialog.h"

typedef struct {
	struct zwlr_foreign_toplevel_handle_v1 *handle;
	char *title;
	gboolean active, minimized, maximized;
} Task;

static GtkWidget *bar, *canvas;
static struct wl_seat *seat;
static struct zwlr_foreign_toplevel_manager_v1 *manager;
static GList *tasks;
static Task *pressed, *hover;
static int first_task;
static GtkWidget *start_menu;
static gboolean start_pressed, start_hover;
static int quick_pressed, quick_hover;
enum { BAR_HEIGHT = 30, START_WIDTH = 100, QUICK_WIDTH = 50, QUICK_BUTTON = 23, CLOCK_WIDTH = 85 };

static void redraw(void) { if (canvas) gtk_widget_queue_draw(canvas); }
static void title(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, const char *s)
{
	Task *t = data;
	g_free(t->title); t->title = g_strdup(s);
}
static void appid(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, const char *s) { }
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
	tasks = g_list_remove(tasks, t);
	zwlr_foreign_toplevel_handle_v1_destroy(h);
	g_free(t->title); g_free(t); redraw();
}
static const struct zwlr_foreign_toplevel_handle_v1_listener task_listener = {
	.title = title, .app_id = appid, .output_enter = output, .output_leave = output,
	.state = state, .done = done, .closed = closed, .parent = parent,
};
static void toplevel(void *data, struct zwlr_foreign_toplevel_manager_v1 *m,
		struct zwlr_foreign_toplevel_handle_v1 *h)
{
	Task *t = g_new0(Task, 1);
	t->handle = h; t->title = g_strdup("Untitled");
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
	pango_cairo_context_set_resolution(pango_layout_get_context(layout), 96);
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
static int task_width(void)
{
	int available = gtk_widget_get_allocated_width(canvas) - START_WIDTH - QUICK_WIDTH - CLOCK_WIDTH - 8;
	return CLAMP(available / MAX(1, (int)g_list_length(tasks)), 70, 160);
}
static int quick_at(double x)
{
	int offset = (int)x - START_WIDTH - 4;
	if (offset < 0 || offset >= QUICK_WIDTH) return 0;
	if (offset < QUICK_BUTTON) return 1;
	if (offset >= QUICK_BUTTON + 2 && offset < 2 * QUICK_BUTTON + 2) return 2;
	return 0;
}
static Task *task_at(double x)
{
	int width = gtk_widget_get_allocated_width(canvas);
	if (x < START_WIDTH + 4 + QUICK_WIDTH || x >= width - CLOCK_WIDTH - 4) return NULL;
	return g_list_nth_data(tasks, first_task + (int)(x - START_WIDTH - 4 - QUICK_WIDTH) / task_width());
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
	GList *l;
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
	if (!luna_draw(cr, "start", start_menu || (start_pressed && start_hover) ? 2 : start_hover ? 1 : 0, 0, 0, START_WIDTH, BAR_HEIGHT)) {
		cairo_set_source_rgb(cr, .15, .53, .15); cairo_rectangle(cr, 0, 1, START_WIDTH, BAR_HEIGHT-1); cairo_fill(cr);
	}
	start_logo(cr, 7, 5);
	cairo_set_source_rgb(cr, 69/255.0, 76/255.0, 16/255.0); text(cr, "start", 36, 4, 61, "Franklin Gothic Medium Italic 14");
	cairo_set_source_rgb(cr, 1, 1, 1); text(cr, "start", 34, 2, 61, "Franklin Gothic Medium Italic 14");
	for (int q = 1; q <= 2; q++) {
		int qx = START_WIDTH + 5 + (q - 1) * (QUICK_BUTTON + 2);
		if (quick_hover == q && !luna_draw(cr, "task-button", quick_pressed == q ? 2 : 1,
			qx, 3, QUICK_BUTTON, BAR_HEIGHT - 6)) {
			cairo_set_source_rgba(cr, .08, .35, .80, .35); cairo_rectangle(cr, qx, 3, QUICK_BUTTON, BAR_HEIGHT - 6); cairo_fill(cr);
		}
		quick_icon(cr, q, qx, 6);
	}
	cairo_set_source_rgba(cr, .04, .18, .50, .8); cairo_rectangle(cr, START_WIDTH + QUICK_WIDTH + 3, 5, 1, BAR_HEIGHT - 10); cairo_fill(cr);
	cairo_save(cr);
	cairo_rectangle(cr, START_WIDTH+4+QUICK_WIDTH, 0, MAX(1, width-START_WIDTH-QUICK_WIDTH-CLOCK_WIDTH-8), BAR_HEIGHT); cairo_clip(cr);
	first_task = CLAMP(first_task, 0, MAX(0, (int)g_list_length(tasks)-1));
	for (l = g_list_nth(tasks, first_task); l; l = l->next, i++) {
		Task *t = l->data;
		x = START_WIDTH + 4 + QUICK_WIDTH + i * tw;
		if (x >= width - CLOCK_WIDTH) break;
		if (!luna_draw(cr, "task-button", t == pressed && t == hover ? 2 :
				t->active ? (t == hover ? 5 : 4) : t == hover ? 1 : 0,
				x, 3, tw-3, BAR_HEIGHT-6)) {
			if (t->active || t == pressed) cairo_set_source_rgb(cr, .03, .20, .60);
			else if (t == hover) cairo_set_source_rgb(cr, .30, .55, .96);
			else cairo_set_source_rgb(cr, .20, .44, .87);
			cairo_rectangle(cr, x+.5, 3.5, tw-3, BAR_HEIGHT-7); cairo_fill_preserve(cr);
			cairo_set_source_rgba(cr, .65, .80, 1, .8); cairo_set_line_width(cr, 1); cairo_stroke(cr);
		}
		cairo_set_source_rgb(cr, .95, .95, .9); cairo_rectangle(cr, x+6, 9, 12, 12); cairo_fill(cr);
		cairo_set_source_rgb(cr, .15, .30, .7); cairo_rectangle(cr, x+7, 10, 10, 3); cairo_fill(cr);
		xp_icon(cr, 3, 16, x+5, 7);
		cairo_set_source_rgb(cr, 1, 1, 1); text(cr, t->title, x+23, 8, tw-29, t->active ? "Tahoma Bold 8" : "Tahoma 8");
	}
	cairo_restore(cr);
	if (!luna_draw(cr, "tray", 0, width-CLOCK_WIDTH, 0, CLOCK_WIDTH, BAR_HEIGHT)) {
		cairo_set_source_rgb(cr, .04, .59, .88); cairo_rectangle(cr, width-CLOCK_WIDTH, 1, CLOCK_WIDTH, BAR_HEIGHT-1); cairo_fill(cr);
	}
	cairo_set_source_rgb(cr, 1, 1, 1); text(cr, clock, width-CLOCK_WIDTH+20, 8, CLOCK_WIDTH-22, "Tahoma 8");
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
	menu_keyboard(FALSE);
	/* Deactivation precedes item activation. Keep items alive until the current
	 * event has finished, including their application/folder callback data. */
	g_idle_add(dispose_menu, g_object_ref(menu));
	redraw();
}

static gboolean button(GtkWidget *w, GdkEventButton *event, gpointer data)
{
	Task *t = task_at(event->x);
	int quick = quick_at(event->x);
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
			GdkRectangle anchor = {0, 0, START_WIDTH, BAR_HEIGHT};
			start_menu = start_menu_new();
			/* A noninteractive layer has no keyboard focus to lend its popup.
			 * Request it only while Start is open; dismissal restores normal focus. */
			menu_keyboard(TRUE);
			gtk_menu_attach_to_widget(GTK_MENU(start_menu), canvas, NULL);
			g_signal_connect(start_menu, "selection-done", G_CALLBACK(menu_closed), NULL);
			g_signal_connect(start_menu, "hide", G_CALLBACK(menu_closed), NULL);
			gtk_menu_popup_at_rect(GTK_MENU(start_menu), gtk_widget_get_window(canvas),
				&anchor, GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_SOUTH_WEST, (GdkEvent *)event);
		}
		if (quick_pressed && quick_pressed == quick_hover) {
			if (quick_pressed == 1) {
				for (GList *l = tasks; l; l = l->next) {
					Task *task = l->data;
					if (!task->minimized) zwlr_foreign_toplevel_handle_v1_set_minimized(task->handle);
				}
			} else {
				GError *error = NULL;
				char *uri = g_filename_to_uri(g_get_home_dir(), NULL, &error);
				if (uri) gtk_show_uri_on_window(GTK_WINDOW(bar), uri, event->time, &error);
				g_free(uri); if (error) g_clear_error(&error);
			}
		}
		if (t && pressed == t && seat) {
			if (t->active && !t->minimized) zwlr_foreign_toplevel_handle_v1_set_minimized(t->handle);
			else zwlr_foreign_toplevel_handle_v1_activate(t->handle, seat);
		}
		pressed = NULL;
		start_pressed = FALSE;
		quick_pressed = 0;
	}
	redraw(); return TRUE;
}
static gboolean motion(GtkWidget *w, GdkEventMotion *event, gpointer data)
{
	hover = task_at(event->x);
	quick_hover = quick_at(event->x);
	start_hover = event->x >= 0 && event->x < START_WIDTH;
	gtk_widget_set_tooltip_text(w, hover ? hover->title : start_hover ? "Start" :
		quick_hover == 1 ? "Show Desktop" : quick_hover == 2 ? "Home" : NULL);
	redraw(); return TRUE;
}
static gboolean leave(GtkWidget *w, GdkEventCrossing *event, gpointer data) { hover = NULL; quick_hover = 0; start_hover = FALSE; redraw(); return TRUE; }
static gboolean scroll(GtkWidget *w, GdkEventScroll *event, gpointer data)
{
	first_task += event->direction == GDK_SCROLL_UP ? -1 : 1; redraw(); return TRUE;
}
static gboolean tick(gpointer data) { redraw(); return G_SOURCE_CONTINUE; }

/* Optional protocol integration check, enabled only by the test runner. */
static gboolean test_tick(gpointer data)
{
	static int stage, tries;
	Task *t = tasks ? tasks->data : NULL;
	if (++tries > 100) g_error("Taskbar protocol test timed out at stage %d", stage);
	if (!t || !seat) return G_SOURCE_CONTINUE;
	if (stage == 0 && t->active) {
		GtkWidget *menu = start_menu_new();
		GList *items = gtk_container_get_children(GTK_CONTAINER(menu));
		/* Header, XP links, recent slots, All Programs/Run and session footer.
		 * The exact number varies with the configured recent-list policy. */
		g_assert_cmpuint(g_list_length(items), >=, 18);
		g_assert(GTK_IS_MENU(g_object_get_data(G_OBJECT(menu), "programs")));
		g_list_free(items);
		gtk_widget_destroy(menu);
		zwlr_foreign_toplevel_handle_v1_set_minimized(t->handle); stage++;
	} else if (stage == 1 && t->minimized && !t->active) {
		zwlr_foreign_toplevel_handle_v1_activate(t->handle, seat); stage++;
	} else if (stage == 2 && t->active && !t->minimized) {
		zwlr_foreign_toplevel_handle_v1_close(t->handle); stage++;
	}
	return G_SOURCE_CONTINUE;
}
static gboolean test_done(gpointer data)
{
	static gboolean had_task;
	if (tasks) had_task = TRUE;
	if (had_task && !tasks) {
		const char *marker = g_getenv("XPWM_TASKBAR_TEST");
		GtkWidget *run = run_dialog_open();
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
			const char *marker = g_getenv("XPWM_SESSION_MARKER");
			char *action = NULL;
			g_assert_cmpstr(gtk_window_get_title(GTK_WINDOW(session)), ==, "Turn off computer");
			gtk_dialog_response(GTK_DIALOG(session), XPWM_SESSION_POWEROFF);
			g_assert(marker && g_file_get_contents(marker, &action, NULL, NULL));
			g_assert_cmpstr(action, ==, "poweroff");
			g_free(action);
		}
		g_assert(g_file_set_contents(marker, "PASS: taskbar enumerate/minimize/activate/close\n", -1, NULL));
		g_print("PASS: Run dialog and session dialog construction, dry-run action, cancel/reopen\n");
		gtk_main_quit(); return G_SOURCE_REMOVE;
	}
	return G_SOURCE_CONTINUE;
}
int main(int argc, char **argv)
{
	struct wl_display *display;
	struct wl_registry *registry;
	/* Custom-drawn chrome must not inherit third-party GTK theme metrics. */
	g_setenv("GTK_THEME", "Adwaita", TRUE);
	luna_init();
	gtk_init(&argc, &argv);
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
	gtk_widget_add_events(canvas, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_SCROLL_MASK);
	g_signal_connect(canvas, "draw", G_CALLBACK(draw), NULL);
	g_signal_connect(canvas, "button-press-event", G_CALLBACK(button), NULL);
	g_signal_connect(canvas, "button-release-event", G_CALLBACK(button), NULL);
	g_signal_connect(canvas, "motion-notify-event", G_CALLBACK(motion), NULL);
	g_signal_connect(canvas, "leave-notify-event", G_CALLBACK(leave), NULL);
	g_signal_connect(canvas, "scroll-event", G_CALLBACK(scroll), NULL);
	g_signal_connect(bar, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_timeout_add_seconds(1, tick, NULL);
	gtk_widget_show_all(bar);
	if (g_getenv("XPWM_TASKBAR_TEST")) {
		g_timeout_add(100, test_tick, NULL); g_timeout_add(100, test_done, NULL);
	}
	gtk_main();
	if (start_menu) gtk_widget_destroy(start_menu);
	while (tasks) { Task *t = tasks->data; closed(t, t->handle); }
	if (seat) wl_seat_destroy(seat);
	zwlr_foreign_toplevel_manager_v1_destroy(manager);
	wl_registry_destroy(registry);
	luna_finish();
	return 0;
}
