#include "../shared/font-settings.h"
/* XP notification-area controls. All service calls run off the GTK event loop. */
#include "tray.h"
#include "xp-style.h"
#include "../shared/luna.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk-layer-shell.h>
#include <gdk/gdkwayland.h>
#include <wayland-client.h>

enum { SLOT = 22, TRAY_LEFT = 19, TRAY_RIGHT = 7 };
#define CLOCK (58*xp_font_dpi()/96)
static gboolean clock_visible = TRUE;
static GtkWidget *tray_canvas, *volume_window, *network_window, *date_window, *power_window;
static GtkWidget *volume_scale, *mute_check, *network_status, *wifi_check, *date_label, *power_label;
static GCancellable *cancellable;
static char *audio_program, *network_program, *battery_path;
static gboolean stopped, updating, audio_ok, network_ok, muted, wifi_enabled;
static gboolean audio_query, network_query, audio_busy, network_busy;
static int volume, desired_volume = -1, desired_mute = -1, capacity = -1;
static char *network_text, *battery_text;
static guint poll_timer, volume_debounce, volume_click_timer;
static GtkWidget *volume_panel, *tray_menu;
static guint audio_revision, audio_query_revision;
static GSubprocess *audio_events;
static GDataInputStream *audio_event_stream;
static guint audio_event_timer;
static gboolean audio_event_pending;
static guint network_revision, network_query_revision;
static guint network_activity;
static GHashTable *traffic;
static GtkWidget *volume_down, *volume_up;
static GtkWidget *brightness_window, *brightness_scale, *brightness_down, *brightness_up, *brightness_label;
static char *brightness_program, *brightness_device;
static const char *brightness_root = "/sys/class/backlight";
static gboolean brightness_ok, brightness_busy, brightness_error;
static gboolean brightness_dbus;
static guint brightness_maximum;
static int brightness, desired_brightness = -1;
static guint brightness_debounce;
static char brightness_tip[160];

typedef struct { guint64 rx, tx; } Traffic;
/* Read counters only for live interfaces; keep a baseline per interface so
 * hotplug, counter resets and interface removal cannot fabricate activity. */
static void read_traffic(const char *root)
{
	GHashTable *next = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	GDir *dir = g_dir_open(root, 0, NULL);
	const char *name;
	network_activity = 0;
	while (dir && (name = g_dir_read_name(dir))) {
		if (!strcmp(name, "lo")) continue;
		char *path = g_build_filename(root, name, "operstate", NULL), *text = NULL;
		gboolean up = g_file_get_contents(path, &text, NULL, NULL) && g_str_has_prefix(text, "up");
		g_free(path); g_free(text);
		if (!up) continue;
		Traffic *value = g_new0(Traffic, 1);
		gboolean valid = TRUE;
		const char *keys[] = {"rx_bytes", "tx_bytes"};
		guint64 *values[] = {&value->rx, &value->tx};
		for (int i = 0; i < 2; i++) {
			path = g_build_filename(root, name, "statistics", keys[i], NULL);
			text = NULL;
			if (g_file_get_contents(path, &text, NULL, NULL)) *values[i] = g_ascii_strtoull(text, NULL, 10);
			else valid = FALSE;
			g_free(path); g_free(text);
		}
		Traffic *old = traffic ? g_hash_table_lookup(traffic, name) : NULL;
		if (valid && old) {
			if (value->rx > old->rx) network_activity |= 1;
			if (value->tx > old->tx) network_activity |= 2;
		}
		if (valid) g_hash_table_insert(next, g_strdup(name), value);
		else g_free(value);
	}
	if (dir) g_dir_close(dir);
	if (traffic) g_hash_table_unref(traffic);
	traffic = next;
}

static const char *network_icon(void)
{
	if (!network_ok || !network_text || !g_str_has_prefix(network_text, "Connected"))
		return "network-disconnected";
	/* XP's LAN activity reflects the link, not an Internet connectivity probe. */
	const char *icons[] = {"network-idle", "network-receive", "network-transmit", "network-connected"};
	return icons[network_activity & 3];
}
static char audio_tip[100], power_tip[160];
static char date_tip[160];

typedef void (*Result)(gboolean ok, const char *output);
typedef struct { GSubprocess *process; guint timeout; Result result; } Command;
static void refresh(void);
static void flush_audio(void);
static void flush_brightness(void);
static void refresh_audio(void);

static gboolean audio_event_refresh(gpointer data)
{
	(void)data; audio_event_timer = 0;
	refresh_audio(); return G_SOURCE_REMOVE;
}

static void audio_event_read(GObject *source, GAsyncResult *result, gpointer data)
{
	(void)data;
	char *line = g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(source), result, NULL, NULL);
	if (stopped) { g_free(line); return; }
	if (!line) {
		g_clear_object(&audio_event_stream); g_clear_object(&audio_events);
		return; /* The existing poll also handles unavailable/restarted servers. */
	}
	if ((strstr(line, " on sink ") || strstr(line, " on server ")) && !audio_event_timer)
		audio_event_timer = g_timeout_add(20, audio_event_refresh, NULL);
	g_free(line);
	g_data_input_stream_read_line_async(G_DATA_INPUT_STREAM(source), G_PRIORITY_DEFAULT,
		cancellable, audio_event_read, NULL);
}

static void subscribe_audio(void)
{
	char *program = g_find_program_in_path("pactl");
	if (!program || !audio_program || audio_events) { g_free(program); return; }
	GSubprocessLauncher *launcher = g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE);
	g_subprocess_launcher_setenv(launcher, "LC_ALL", "C", TRUE);
	audio_events = g_subprocess_launcher_spawn(launcher, NULL, program, "subscribe", NULL);
	g_object_unref(launcher); g_free(program);
	if (!audio_events) return;
	audio_event_stream = g_data_input_stream_new(g_subprocess_get_stdout_pipe(audio_events));
	g_data_input_stream_read_line_async(audio_event_stream, G_PRIORITY_DEFAULT,
		cancellable, audio_event_read, NULL);
}

static gboolean command_timeout(gpointer data)
{
	Command *command = data;
	command->timeout = 0;
	g_subprocess_force_exit(command->process);
	return G_SOURCE_REMOVE;
}

static void completed(GObject *object, GAsyncResult *result, gpointer data)
{
	Command *command = data;
	char *output = NULL, *errors = NULL;
	GError *error = NULL;
	gboolean ok = g_subprocess_communicate_utf8_finish(command->process, result, &output, &errors, &error);
	if (command->timeout) g_source_remove(command->timeout);
	ok = ok && g_subprocess_get_successful(command->process);
	if (!stopped && command->result) command->result(ok, ok ? output : (errors && *errors ? errors : "Service unavailable"));
	g_clear_error(&error); g_free(output); g_free(errors);
	g_object_unref(command->process); g_free(command);
}

static void run(const char *const *argv, Result result)
{
	GError *error = NULL;
	GSubprocessLauncher *launcher = g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE);
	g_subprocess_launcher_setenv(launcher, "LC_ALL", "C", TRUE);
	GSubprocess *process = g_subprocess_launcher_spawnv(launcher, argv, &error);
	g_object_unref(launcher);
	if (!process) {
		if (result) result(FALSE, error ? error->message : "Service unavailable");
		g_clear_error(&error); return;
	}
	Command *command = g_new0(Command, 1);
	command->process = process; command->result = result;
	command->timeout = g_timeout_add_seconds(5, command_timeout, command);
	g_subprocess_communicate_utf8_async(process, NULL, cancellable, completed, command);
}

static void update_ui(void)
{
	updating = TRUE;
	if (date_window) {
		GDateTime *now = g_date_time_new_now_local();
		char *date = g_date_time_format(now, "%A, %B %e, %Y\n%X %Z");
		gtk_label_set_text(GTK_LABEL(date_label), date);
		g_free(date); g_date_time_unref(now);
	}
	if (volume_window) {
		gtk_widget_set_sensitive(volume_scale, audio_ok);
		gtk_widget_set_sensitive(mute_check, audio_ok);
		gtk_widget_set_sensitive(volume_down, audio_ok && volume > 0);
		gtk_widget_set_sensitive(volume_up, audio_ok && volume < 100);
		gtk_range_set_value(GTK_RANGE(volume_scale), volume);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mute_check), muted);
		char *status = audio_ok ? g_strdup_printf("%d%%", volume) : g_strdup("Audio service unavailable");
		gtk_widget_set_tooltip_text(volume_scale, status); g_free(status);
	}
	if (network_window) {
		gtk_label_set_text(GTK_LABEL(network_status), network_text ? network_text : "Checking connection…");
		gtk_widget_set_sensitive(wifi_check, network_ok && !network_busy);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wifi_check), wifi_enabled);
	}
	if (power_window) gtk_label_set_text(GTK_LABEL(power_label), battery_text ? battery_text : "Battery unavailable");
	if (brightness_window) {
		gtk_widget_set_sensitive(brightness_scale, brightness_ok);
		gtk_widget_set_sensitive(brightness_down, brightness_ok && brightness > 1);
		gtk_widget_set_sensitive(brightness_up, brightness_ok && brightness < 100);
		gtk_range_set_value(GTK_RANGE(brightness_scale), brightness);
		char *text = !brightness_program ? g_strdup("Brightness controls unavailable.") :
			brightness_error ? g_strdup("Unable to change brightness. Check backlight permissions.") :
			brightness_ok ? g_strdup_printf("Screen brightness: %d%%", brightness) : g_strdup("Screen brightness unavailable");
		gtk_label_set_text(GTK_LABEL(brightness_label), text); g_free(text);
	}
	updating = FALSE;
	if (tray_canvas) gtk_widget_queue_draw(tray_canvas);
}

static void got_audio(gboolean ok, const char *output)
{
	audio_query = FALSE;
	if (audio_query_revision != audio_revision) { refresh_audio(); return; }
	if (audio_busy || desired_volume >= 0 || desired_mute >= 0) return;
	double level = 0;
	audio_ok = ok && output && sscanf(output, "Volume: %lf", &level) == 1 && isfinite(level) && level >= 0;
	if (audio_ok) { volume = (int)round(MIN(level*100, 1000)); muted = strstr(output, "[MUTED]") != NULL; }
	update_ui();
	/* Show each completed read even during held-key repeats, then catch up to
	 * notifications received while that read was in flight. */
	if (audio_event_pending) refresh_audio();
}

static void got_network(gboolean ok, const char *output)
{
	network_query = FALSE;
	if (network_query_revision != network_revision) { refresh(); return; }
	if (network_busy) return;
	network_ok = ok && output && *output;
	g_free(network_text);
	if (network_ok) {
		char **fields = g_strsplit(output, ":", 3);
		const char *state = fields[0];
		gboolean connected = g_str_has_prefix(state, "connected");
		const char *access = fields[1] && !strcmp(fields[1], "full") ? "Internet access" :
			fields[1] && !strcmp(fields[1], "portal") ? "Sign-in required" : "Limited connectivity";
		wifi_enabled = g_strv_length(fields) >= 3 && g_str_has_prefix(fields[2], "enabled");
		network_text = g_strdup_printf("%s\n%s", connected ? "Connected" : "Disconnected", connected ? access : "No active network connection");
		g_strfreev(fields);
	} else network_text = g_strdup("NetworkManager unavailable");
	update_ui();
}

static void read_battery(void)
{
	if (!battery_path) return;
	char *path = g_build_filename(battery_path, "capacity", NULL), *value = NULL, *status = NULL;
	capacity = -1;
	if (g_file_get_contents(path, &value, NULL, NULL)) capacity = CLAMP(atoi(value), 0, 100);
	g_free(path); g_free(value);
	path = g_build_filename(battery_path, "status", NULL);
	g_file_get_contents(path, &status, NULL, NULL); g_free(path);
	if (status) g_strstrip(status);
	g_free(battery_text);
	battery_text = capacity >= 0 ? g_strdup_printf("Battery: %d%%\n%s", capacity, status ? status : "Unknown") : g_strdup("Battery unavailable");
	g_free(status);
}

static void refresh_audio(void)
{
	if (!audio_program) return;
	if (audio_query || audio_busy) { audio_event_pending = TRUE; return; }
	audio_event_pending = FALSE;
	audio_query = TRUE;
	audio_query_revision = audio_revision;
	const char *argv[] = {audio_program, "get-volume", "@DEFAULT_AUDIO_SINK@", NULL};
	run(argv, got_audio);
}

static void refresh(void)
{
	read_traffic("/sys/class/net");
	if (brightness_device && !brightness_busy && desired_brightness < 0) {
		char *path = g_build_filename(brightness_root, brightness_device, "brightness", NULL), *value = NULL;
		gboolean current_ok = g_file_get_contents(path, &value, NULL, NULL);
		double current = value ? g_ascii_strtod(value, NULL) : -1;
		g_free(path); g_free(value); value = NULL;
		path = g_build_filename(brightness_root, brightness_device, "max_brightness", NULL);
		gboolean maximum_ok = g_file_get_contents(path, &value, NULL, NULL);
		double maximum = value ? g_ascii_strtod(value, NULL) : 0;
		brightness_ok = brightness_program && current_ok && maximum_ok && isfinite(current) && isfinite(maximum) && current >= 0 && maximum > 0 && maximum <= G_MAXUINT;
		brightness_maximum = brightness_ok ? (guint)maximum : 0;
		if (brightness_ok) brightness = CLAMP((int)round(current * 100 / maximum), 0, 100);
		g_free(path); g_free(value);
	}
	refresh_audio();
	subscribe_audio();
	if (network_program && !network_query && !network_busy) {
		network_query = TRUE;
		network_query_revision = network_revision;
		const char *argv[] = {network_program, "-t", "-f", "STATE,CONNECTIVITY,WIFI", "general", "status", NULL};
		run(argv, got_network);
	}
	read_battery(); update_ui();
}

static gboolean poll(gpointer data) { refresh(); return G_SOURCE_CONTINUE; }

static void audio_changed(gboolean ok, const char *output)
{
	audio_busy = FALSE;
	if (!ok) {
		desired_volume = desired_mute = -1;
		audio_ok = FALSE;
		update_ui();
		if (volume_window) gtk_widget_set_tooltip_text(volume_scale, "Unable to change volume");
		return;
	}
	flush_audio();
}

static void flush_audio(void)
{
	if (audio_busy || !audio_program) return;
	char value[32]; const char *operation;
	if (desired_volume >= 0) {
		g_snprintf(value, sizeof(value), "%d%%", desired_volume); desired_volume = -1;
		operation = "set-volume";
	} else if (desired_mute >= 0) {
		g_snprintf(value, sizeof(value), "%d", desired_mute); desired_mute = -1;
		operation = "set-mute";
	} else { refresh(); return; }
	audio_busy = TRUE;
	audio_revision++;
	const char *argv[] = {audio_program, operation, "@DEFAULT_AUDIO_SINK@", value, NULL};
	run(argv, audio_changed);
}

static gboolean set_volume_later(gpointer data) { volume_debounce = 0; flush_audio(); return G_SOURCE_REMOVE; }

static void volume_changed(GtkRange *range, gpointer data)
{
	if (updating) return;
	volume = desired_volume = CLAMP((int)gtk_range_get_value(range), 0, 100);
	if (volume_debounce) g_source_remove(volume_debounce);
	volume_debounce = g_timeout_add(100, set_volume_later, NULL);
}

static void mute_changed(GtkToggleButton *button, gpointer data)
{
	if (updating) return;
	muted = gtk_toggle_button_get_active(button); desired_mute = muted;
	flush_audio();
}

static void volume_step(GtkButton *button, gpointer data)
{
	if (!audio_ok) return;
	volume = desired_volume = CLAMP(volume + GPOINTER_TO_INT(data), 0, 100);
	flush_audio(); update_ui();
}

static void brightness_done(gboolean ok, const char *output)
{
	brightness_busy = FALSE; brightness_error = !ok;
	if (!ok) { desired_brightness = -1; refresh(); return; }
	flush_brightness();
}

static void flush_brightness(void)
{
	if (brightness_busy || !brightness_program || !brightness_device) return;
	if (desired_brightness < 0) { refresh(); return; }
	char value[32];
	if (brightness_dbus) g_snprintf(value, sizeof(value), "%u", (guint)ceil(brightness_maximum * (desired_brightness / 100.0)));
	else g_snprintf(value, sizeof(value), "%d%%", desired_brightness);
	desired_brightness = -1; brightness_busy = TRUE;
	const char *argv[] = {brightness_program, "--class=backlight", "--device", brightness_device, "set", value, NULL};
	const char *dbus_argv[] = {brightness_program, "call", "--system", "--dest", "org.freedesktop.login1",
		"--object-path", "/org/freedesktop/login1/session/self", "--method",
		"org.freedesktop.login1.Session.SetBrightness", "backlight", brightness_device, value, NULL};
	run(brightness_dbus ? dbus_argv : argv, brightness_done);
}

static gboolean brightness_later(gpointer data)
{
	brightness_debounce = 0; flush_brightness(); return G_SOURCE_REMOVE;
}

static void brightness_changed(GtkRange *range, gpointer data)
{
	if (updating || !brightness_ok) return;
	brightness = desired_brightness = CLAMP((int)gtk_range_get_value(range), 1, 100);
	if (brightness_debounce) g_source_remove(brightness_debounce);
	brightness_debounce = g_timeout_add(100, brightness_later, NULL);
	update_ui();
}

static void brightness_step(GtkButton *button, gpointer data)
{
	if (!brightness_ok) return;
	brightness = desired_brightness = CLAMP(brightness + GPOINTER_TO_INT(data), 1, 100);
	flush_brightness(); update_ui();
}

static void wifi_changed_done(gboolean ok, const char *output)
{
	network_busy = FALSE;
	if (!ok) {
		g_free(network_text); network_text = g_strdup("Unable to change Wi-Fi. Check your network permissions.");
		update_ui(); return;
	}
	refresh();
}

static void wifi_changed(GtkToggleButton *button, gpointer data)
{
	if (updating || !network_program || network_busy) return;
	wifi_enabled = gtk_toggle_button_get_active(button); network_busy = TRUE;
	network_revision++;
	const char *argv[] = {network_program, "--wait", "3", "radio", "wifi", wifi_enabled ? "on" : "off", NULL};
	update_ui(); run(argv, wifi_changed_done);
}

static void open_tool(GtkButton *button, gpointer name)
{
	char *argv[] = {name, NULL};
	GError *error = NULL;
	/* A settings application's lifetime belongs to the user, not query timeout. */
	if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error)) {
		GtkWidget *message = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE, "Unable to open settings");
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(message), "%s", error->message);
		g_signal_connect_swapped(message, "response", G_CALLBACK(gtk_widget_destroy), message);
		xp_style_apply(message); gtk_widget_show(message); g_error_free(error);
	}
}

static GtkWidget *dialog(GtkWidget **slot, const char *title, GtkWidget **box)
{
	if (*slot) { gtk_window_present(GTK_WINDOW(*slot)); return NULL; }
	*slot = gtk_dialog_new_with_buttons(title, NULL, 0, "Close", GTK_RESPONSE_CLOSE, NULL);
	gtk_window_set_resizable(GTK_WINDOW(*slot), FALSE);
	gtk_window_set_default_size(GTK_WINDOW(*slot), 280, -1);
	g_signal_connect(*slot, "destroy", G_CALLBACK(gtk_widget_destroyed), slot);
	g_signal_connect_swapped(*slot, "response", G_CALLBACK(gtk_widget_destroy), *slot);
	*box = gtk_dialog_get_content_area(GTK_DIALOG(*slot));
	gtk_container_set_border_width(GTK_CONTAINER(*box), 12); gtk_box_set_spacing(GTK_BOX(*box), 10);
	xp_style_apply(*slot); return *slot;
}

static void tool_button(GtkWidget *box, const char *label, const char *program)
{
	char *path = g_find_program_in_path(program);
	if (!path) return;
	GtkWidget *button = gtk_button_new_with_label(label);
	g_signal_connect(button, "clicked", G_CALLBACK(open_tool), (gpointer)program);
	gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0); g_free(path);
}

static void close_volume(void)
{
 if (volume_window) gtk_widget_destroy(volume_window);
 volume_panel = NULL;
}

static gboolean popup_key(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
 if (event->keyval == GDK_KEY_Escape) { close_volume(); return TRUE; }
 return FALSE;
}

static gboolean popup_outside(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
 if (!volume_panel) return FALSE;
 GdkWindow *source = event->window;
 GdkWindow *panel_window = gtk_widget_get_window(volume_panel);
 GdkWindow *root_window = gtk_widget_get_window(widget);
 while (source && source != root_window) {
  if (source == panel_window) return FALSE;
  source = gdk_window_get_parent(source);
 }
 int x, y;
 gtk_widget_translate_coordinates(volume_panel, widget, 0, 0, &x, &y);
 if (event->x >= x && event->x < x+gtk_widget_get_allocated_width(volume_panel) &&
     event->y >= y && event->y < y+gtk_widget_get_allocated_height(volume_panel)) return FALSE;
 close_volume(); return TRUE;
}

static gboolean popup_clear(GtkWidget *widget, cairo_t *cr, gpointer data)
{
 cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE); cairo_set_source_rgba(cr, 0, 0, 0, 0);
 cairo_paint(cr); cairo_set_operator(cr, CAIRO_OPERATOR_OVER); return FALSE;
}

static gboolean volume_thumb(GtkWidget *widget, cairo_t *cr, gpointer data)
{
 gint start, end;
 GdkRectangle range;
 GtkStateFlags flags = gtk_widget_get_state_flags(widget);
 int state = !gtk_widget_get_sensitive(widget) ? 4 : (flags & GTK_STATE_FLAG_ACTIVE) ? 2 :
  (flags & GTK_STATE_FLAG_PRELIGHT) ? 1 : 0;
 gtk_range_get_slider_range(GTK_RANGE(widget), &start, &end);
 gtk_range_get_range_rect(GTK_RANGE(widget), &range);
 int center = gtk_widget_get_allocated_width(widget)/2;
 int top = range.y+5, bottom = range.y+range.height-6;
 /* Paint the narrow Win32 track ourselves; host GTK themes otherwise make
  * the trough as wide as the thumb and add a modern filled progress area. */
 cairo_set_line_width(cr, 1);
 cairo_set_source_rgb(cr, .55, .55, .52); cairo_rectangle(cr, center-2, top, 4, bottom-top); cairo_fill(cr);
 cairo_set_source_rgb(cr, 1, 1, 1); cairo_rectangle(cr, center-1, top+1, 2, bottom-top-2); cairo_fill(cr);
 cairo_set_source_rgb(cr, .25, .25, .25);
 for (int i = 0; i < 5; i++) {
  int y = top+(bottom-top)*i/4;
  cairo_rectangle(cr, center-18, y, 5, 1); cairo_fill(cr);
 }
 xp_bitmap_region(cr, "xp-tray/volume-thumb.png", 0, state*11, 21, 11,
  center-10, (start+end-11)/2);
 return TRUE;
}

static void open_volume(void)
{
 if (volume_window) { close_volume(); return; }
 volume_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
 gtk_window_set_title(GTK_WINDOW(volume_window), "Volume");
 gtk_widget_set_name(volume_window, "xpwm-volume-popup");
 gtk_widget_set_app_paintable(volume_window, TRUE);
 GdkVisual *visual = gdk_screen_get_rgba_visual(gtk_widget_get_screen(volume_window));
 if (visual) gtk_widget_set_visual(volume_window, visual);
 gtk_layer_init_for_window(GTK_WINDOW(volume_window));
 gtk_layer_set_namespace(GTK_WINDOW(volume_window), "xpwm-volume-popup");
 gtk_layer_set_layer(GTK_WINDOW(volume_window), GTK_LAYER_SHELL_LAYER_OVERLAY);
 gtk_layer_set_exclusive_zone(GTK_WINDOW(volume_window), -1);
 gtk_layer_set_keyboard_mode(GTK_WINDOW(volume_window), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
 for (int edge = 0; edge < GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER; edge++)
  gtk_layer_set_anchor(GTK_WINDOW(volume_window), edge, TRUE);
 if (tray_canvas && gtk_widget_get_window(tray_canvas)) {
  GdkMonitor *monitor = gdk_display_get_monitor_at_window(gtk_widget_get_display(tray_canvas), gtk_widget_get_window(tray_canvas));
  if (monitor) gtk_layer_set_monitor(GTK_WINDOW(volume_window), monitor);
 }
 g_signal_connect(volume_window, "destroy", G_CALLBACK(gtk_widget_destroyed), &volume_window);
 g_signal_connect(volume_window, "draw", G_CALLBACK(popup_clear), NULL);
 g_signal_connect(volume_window, "key-press-event", G_CALLBACK(popup_key), NULL);
 gtk_widget_add_events(volume_window, GDK_BUTTON_PRESS_MASK);
 g_signal_connect(volume_window, "button-press-event", G_CALLBACK(popup_outside), NULL);
 GtkWidget *overlay = gtk_overlay_new(); gtk_container_add(GTK_CONTAINER(volume_window), overlay);
 /* SNDVOL32 dialog 106: 50x80 dialog units at the XP 96-DPI
  * MS Shell Dlg 8 metrics (base units 6x13): 75x130 client pixels. */
 volume_panel = gtk_event_box_new();
 gtk_widget_set_name(volume_panel, "xpwm-volume-panel");
 gtk_widget_set_halign(volume_panel, GTK_ALIGN_END); gtk_widget_set_valign(volume_panel, GTK_ALIGN_END);
 gtk_widget_set_margin_bottom(volume_panel, 30);
 gtk_widget_set_margin_end(volume_panel, MAX(7, xp_tray_width()-TRAY_LEFT-SLOT/2-38));
 GtkWidget *fixed = gtk_fixed_new();
 gtk_widget_set_size_request(fixed, 75, 170);
 gtk_container_add(GTK_CONTAINER(volume_panel), fixed);
 gtk_overlay_add_overlay(GTK_OVERLAY(overlay), volume_panel);
 GtkWidget *title = gtk_label_new("Volume");
 gtk_widget_set_size_request(title, 75, 13); gtk_fixed_put(GTK_FIXED(fixed), title, 0, 6);
 volume_scale = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0, 100, 1);
 gtk_range_set_inverted(GTK_RANGE(volume_scale), TRUE);
 gtk_scale_set_draw_value(GTK_SCALE(volume_scale), FALSE);
 gtk_widget_set_size_request(volume_scale, 42, 81);
 gtk_fixed_put(GTK_FIXED(fixed), volume_scale, 17, 24);
 mute_check = gtk_check_button_new_with_mnemonic("_Mute");
 volume_down = gtk_button_new_with_label("−"); volume_up = gtk_button_new_with_label("+");
 gtk_widget_set_size_request(volume_down, 30, 25); gtk_widget_set_size_request(volume_up, 30, 25);
 gtk_widget_set_tooltip_text(volume_down, "Lower volume by 5%");
 gtk_widget_set_tooltip_text(volume_up, "Raise volume by 5%");
 gtk_fixed_put(GTK_FIXED(fixed), volume_down, 5, 110);
 gtk_fixed_put(GTK_FIXED(fixed), volume_up, 40, 110);
 g_signal_connect(volume_down, "clicked", G_CALLBACK(volume_step), GINT_TO_POINTER(-5));
 g_signal_connect(volume_up, "clicked", G_CALLBACK(volume_step), GINT_TO_POINTER(5));
 gtk_fixed_put(GTK_FIXED(fixed), mute_check, 6, 145);
 static GtkCssProvider *css;
 if (!css) {
 css = gtk_css_provider_new();
 gtk_css_provider_load_from_data(css,
  "#xpwm-volume-popup, #xpwm-volume-popup * { font:8pt Tahoma; color:#000; text-shadow:none; box-shadow:none; }"
  "#xpwm-volume-popup { background:transparent; }"
  "#xpwm-volume-panel { background:#ece9d8; border:1px solid #808080; border-radius:0; padding:1px; }"
  "#xpwm-volume-popup scale { padding:5px; }"
  "#xpwm-volume-popup scale trough { background:#f1efe2; border:1px solid #9d9c99; min-width:2px; }"
  "#xpwm-volume-popup scale slider { background:transparent; background-image:none; border:0; box-shadow:none; min-width:21px; min-height:11px; margin:0; }"
  "#xpwm-volume-popup checkbutton { padding:0; }"
  "#xpwm-volume-popup check { min-width:13px; min-height:13px; border:1px solid #1c5180; border-radius:0; background:#fff; margin-right:4px; }"
  "#xpwm-volume-popup check:checked { color:#21a121; -gtk-icon-source:-gtk-icontheme(\"object-select-symbolic\"); }", -1, NULL);
 gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(volume_window), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER+20);
 }
 g_signal_connect(volume_scale, "draw", G_CALLBACK(volume_thumb), NULL);
 g_signal_connect(volume_scale, "value-changed", G_CALLBACK(volume_changed), NULL);
 g_signal_connect(mute_check, "toggled", G_CALLBACK(mute_changed), NULL);
 refresh(); gtk_widget_show_all(volume_window); gtk_widget_grab_focus(volume_scale);
}

static gboolean volume_single_click(gpointer data)
{
 volume_click_timer = 0; open_volume(); return G_SOURCE_REMOVE;
}

static void open_network(void)
{
	GtkWidget *box, *window = dialog(&network_window, "Network Connection Status", &box);
	if (!window) return;
	network_status = gtk_label_new(NULL); gtk_label_set_line_wrap(GTK_LABEL(network_status), TRUE);
	gtk_box_pack_start(GTK_BOX(box), network_status, FALSE, FALSE, 0);
	wifi_check = gtk_check_button_new_with_mnemonic("Enable _wireless networking");
	gtk_box_pack_start(GTK_BOX(box), wifi_check, FALSE, FALSE, 0);
	g_signal_connect(wifi_check, "toggled", G_CALLBACK(wifi_changed), NULL);
	tool_button(box, "Network Connections…", "nm-connection-editor");
	refresh(); xp_style_apply(window); gtk_widget_show_all(window);
}

static void open_date(void)
{
	GtkWidget *box, *window = dialog(&date_window, "Date and Time", &box);
	if (!window) return;
	GDateTime *now = g_date_time_new_now_local();
	char *date = g_date_time_format(now, "%A, %B %e, %Y\n%X %Z");
	date_label = gtk_label_new(date); g_free(date); g_date_time_unref(now);
	gtk_box_pack_start(GTK_BOX(box), date_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), gtk_calendar_new(), FALSE, FALSE, 0);
	gtk_widget_show_all(window);
}

static void open_power(void)
{
	GtkWidget *box, *window = dialog(&power_window, "Power Meter", &box);
	if (!window) return;
	power_label = gtk_label_new(NULL); gtk_box_pack_start(GTK_BOX(box), power_label, FALSE, FALSE, 0);
	refresh(); gtk_widget_show_all(window);
}

static void open_brightness(void)
{
	GtkWidget *box, *window = dialog(&brightness_window, "Screen Brightness", &box);
	if (!window) return;
	brightness_label = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(brightness_label), TRUE);
	gtk_box_pack_start(GTK_BOX(box), brightness_label, FALSE, FALSE, 0);
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	brightness_down = gtk_button_new_with_label("−"); brightness_up = gtk_button_new_with_label("+");
	gtk_widget_set_tooltip_text(brightness_down, "Lower brightness by 10%");
	gtk_widget_set_tooltip_text(brightness_up, "Raise brightness by 10%");
	brightness_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1, 100, 1);
	gtk_scale_set_draw_value(GTK_SCALE(brightness_scale), FALSE);
	gtk_box_pack_start(GTK_BOX(row), brightness_down, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(row), brightness_scale, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(row), brightness_up, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), row, FALSE, FALSE, 0);
	g_signal_connect(brightness_down, "clicked", G_CALLBACK(brightness_step), GINT_TO_POINTER(-10));
	g_signal_connect(brightness_up, "clicked", G_CALLBACK(brightness_step), GINT_TO_POINTER(10));
	g_signal_connect(brightness_scale, "value-changed", G_CALLBACK(brightness_changed), NULL);
	refresh(); gtk_widget_show_all(window);
}

static int tray_slots(void) { return 2 + !!battery_path + !!brightness_device; }

void xp_tray_set_clock_visible(gboolean visible) { clock_visible = visible; }
gboolean xp_tray_popup_open(void) { return volume_window || tray_menu; }
int xp_tray_width(void) { return TRAY_LEFT + SLOT*tray_slots() + (clock_visible ? CLOCK : 0) + TRAY_RIGHT; }

/* The theme's ContentMargins are 19,7,0,0. Every original icon stays 16x16;
 * toolbar slots have three pixels either side, with no image resampling. */
int xp_tray_hit(int x)
{
	if (x < TRAY_LEFT || x >= xp_tray_width()) return -1;
	int slot = (x - TRAY_LEFT) / SLOT;
	if (slot < 2) return slot;
	if (battery_path && slot == 2) return 2;
	if (brightness_device && slot == 2 + !!battery_path) return 4;
	return clock_visible ? 3 : -1;
}

int xp_tray_clock_x(void) { return xp_tray_width() - xp_tray_clock_width() - TRAY_RIGHT; }
int xp_tray_clock_width(void) { return clock_visible ? CLOCK : 0; }

static void original_icon(cairo_t *cr, const char *name, int x, int y)
{
	char *path = g_strdup_printf("xp-tray/%s.png", name);
	if (!xp_bitmap(cr, path, x, y)) {
	/* Honest missing-asset placeholder, not a substitute for XP artwork. */
	cairo_set_source_rgb(cr, .8, .8, .8); cairo_rectangle(cr, x+2, y+2, 12, 12); cairo_fill(cr);
	}
	g_free(path);
}

/* Original BATMETER.DLL bitmap frames, selected without repainting artwork. */
static void battery_icon(cairo_t *cr,int x,int y)
{
 const char *role=capacity<0 ? "power-unknown" :
  battery_text && strstr(battery_text,"Charging") ? "power-charging" :
  battery_text && strstr(battery_text,"Full") ? "power-ac" :
  capacity<20 ? "power-low" : capacity<60 ? "power-medium" : "power-battery";
 original_icon(cr,role,x,y);
}

void xp_tray_draw(cairo_t *cr, int x, int height)
{
	int y = (height-16)/2;
	int icon_x = x + TRAY_LEFT + (SLOT-16)/2;
	original_icon(cr, muted || !audio_ok ? "volume-muted" : "volume", icon_x, y);
	original_icon(cr, network_icon(), icon_x+SLOT, y);
	if (battery_path) battery_icon(cr,icon_x+2*SLOT,y);
	if (brightness_device)
  original_icon(cr,"display-brightness",icon_x+(2+!!battery_path)*SLOT,y);
}

const char *xp_tray_tooltip(int x)
{
	if (x < 0 || x >= xp_tray_width()) return NULL;
	int slot = xp_tray_hit(x);
	if (slot < 0) return NULL;
	if (slot == 4) {
		g_snprintf(brightness_tip, sizeof(brightness_tip), "Screen brightness: %d%%%s", brightness,
			brightness_program ? "" : " (controls unavailable)");
		return brightness_tip;
	}
	if (slot == 0) {
		g_snprintf(audio_tip, sizeof(audio_tip), audio_ok ? "Volume: %d%%%s" : "Audio service unavailable", volume, muted ? " (muted)" : "");
		return audio_tip;
	}
	if (slot == 1) return network_text ? network_text : "Checking network…";
	if (slot == 2 && battery_path) {
		g_snprintf(power_tip, sizeof(power_tip), "%s", battery_text ? battery_text : "Power Meter"); return power_tip;
	}
	GDateTime *now = g_date_time_new_now_local();
	char *date = g_date_time_format(now, "%A, %B %e, %Y");
	g_snprintf(date_tip, sizeof(date_tip), "%s", date);
	g_free(date); g_date_time_unref(now); return date_tip;
}

static void tray_menu_keyboard(gboolean grab)
{
 if (!tray_canvas) return;
 GtkWidget *window = gtk_widget_get_toplevel(tray_canvas);
 if (!GTK_IS_WINDOW(window) || !gtk_layer_is_layer_window(GTK_WINDOW(window))) return;
 gtk_layer_set_keyboard_mode(GTK_WINDOW(window), grab ? GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE : GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
 GdkWindow *gdk = gtk_widget_get_window(window);
 if (gdk && GDK_IS_WAYLAND_WINDOW(gdk)) wl_surface_commit(gdk_wayland_window_get_wl_surface(gdk));
}

static gboolean dispose_tray_menu(gpointer menu) { gtk_widget_destroy(menu); return G_SOURCE_REMOVE; }
static void tray_menu_closed(GtkWidget *menu, gpointer data)
{
 tray_menu = NULL; tray_menu_keyboard(FALSE); g_idle_add(dispose_tray_menu, menu);
}
static void menu_volume(GtkMenuItem *item, gpointer data) { open_tool(NULL, "pavucontrol"); }
static void menu_sound(GtkMenuItem *item, gpointer data) { open_tool(NULL, "pavucontrol"); }
static void menu_network(GtkMenuItem *item, gpointer data) { open_network(); }
static void menu_connections(GtkMenuItem *item, gpointer data) { open_tool(NULL, "nm-connection-editor"); }
static void menu_date(GtkMenuItem *item, gpointer data) { open_date(); }
static void menu_power(GtkMenuItem *item, gpointer data) { open_power(); }
static void menu_brightness(GtkMenuItem *item, gpointer data) { open_brightness(); }
static void menu_entry(GtkWidget *menu, const char *label, GCallback callback)
{
 GtkWidget *item = gtk_menu_item_new_with_label(label);
 g_signal_connect(item, "activate", callback, NULL); gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
}
static void context_menu(int slot)
{
 if (tray_menu) return;
 close_volume();
 tray_menu = gtk_menu_new();
 if (slot == 0) {
  menu_entry(tray_menu, "Open Volume Control", G_CALLBACK(menu_volume));
  menu_entry(tray_menu, "Adjust Audio Properties", G_CALLBACK(menu_sound));
 } else if (slot == 1) {
  menu_entry(tray_menu, "Status", G_CALLBACK(menu_network));
  menu_entry(tray_menu, "Open Network Connections", G_CALLBACK(menu_connections));
 } else if (slot == 2) menu_entry(tray_menu, "Open Power Meter", G_CALLBACK(menu_power));
 else if (slot == 4) menu_entry(tray_menu, "Adjust Screen Brightness", G_CALLBACK(menu_brightness));
 else menu_entry(tray_menu, "Adjust Date/Time", G_CALLBACK(menu_date));
 xp_style_apply(tray_menu); gtk_widget_show_all(tray_menu);
 g_signal_connect(tray_menu, "selection-done", G_CALLBACK(tray_menu_closed), NULL);
 if (tray_canvas && gtk_widget_get_window(tray_canvas)) {
  tray_menu_keyboard(TRUE);
  GdkRectangle anchor = {gtk_widget_get_allocated_width(tray_canvas)-xp_tray_width()+TRAY_LEFT+MIN(slot, 2)*SLOT, 0, SLOT, 30};
  gtk_menu_attach_to_widget(GTK_MENU(tray_menu), tray_canvas, NULL);
  gtk_menu_popup_at_rect(GTK_MENU(tray_menu), gtk_widget_get_window(tray_canvas), &anchor,
   GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_SOUTH_WEST, NULL);
 }
}

void xp_tray_double_click(int x)
{
 if (volume_click_timer) { g_source_remove(volume_click_timer); volume_click_timer = 0; }
 int slot = xp_tray_hit(x);
 if (slot < 0) return;
 if (slot == 0) { close_volume(); open_tool(NULL, "pavucontrol"); }
 else if (slot == 1) open_network();
 else if (slot == 2) open_power();
 else if (slot == 3) open_date();
 else if (slot == 4) open_brightness();
}

void xp_tray_click(int x, guint button)
{
 if (x < 0 || x >= xp_tray_width() || (button != 1 && button != 3)) return;
 int slot = xp_tray_hit(x);
 if (slot < 0) return;
 if (volume_click_timer) { g_source_remove(volume_click_timer); volume_click_timer = 0; }
 if (button == 3) { context_menu(slot); return; }
 if (slot == 0) {
  gint delay = 250;
  g_object_get(gtk_settings_get_default(), "gtk-double-click-time", &delay, NULL);
  volume_click_timer = g_timeout_add(delay+10, volume_single_click, NULL);
 } else if (slot == 4) open_brightness();
 else if (slot == 1) open_network();
 else if (slot == 2 && battery_path) open_power();
 else open_date();
}

gboolean xp_tray_scroll(int x, GdkScrollDirection direction)
{
	if (xp_tray_hit(x) == 4) {
		if (direction != GDK_SCROLL_UP && direction != GDK_SCROLL_DOWN) return FALSE;
		brightness_step(NULL, GINT_TO_POINTER(direction == GDK_SCROLL_UP ? 10 : -10)); return TRUE;
	}
	if (xp_tray_hit(x) != 0) return FALSE;
	if (audio_ok && (direction == GDK_SCROLL_UP || direction == GDK_SCROLL_DOWN)) {
		volume = desired_volume = CLAMP(volume + (direction == GDK_SCROLL_UP ? 5 : -5), 0, 100);
		flush_audio(); update_ui();
	}
	return TRUE;
}

void xp_tray_init(GtkWidget *canvas)
{
	tray_canvas = canvas; stopped = FALSE; cancellable = g_cancellable_new();
	audio_program = g_find_program_in_path("wpctl"); network_program = g_find_program_in_path("nmcli");
	brightness_program = g_find_program_in_path("brightnessctl");
	brightness_dbus = brightness_program == NULL;
	if (brightness_dbus) brightness_program = g_find_program_in_path("gdbus");
	GDir *backlights = g_dir_open(brightness_root, 0, NULL);
	const char *backlight = backlights ? g_dir_read_name(backlights) : NULL;
	if (backlight) brightness_device = g_strdup(backlight);
	if (backlights) g_dir_close(backlights);
	GDir *dir = g_dir_open("/sys/class/power_supply", 0, NULL);
	const char *name;
	while (dir && (name = g_dir_read_name(dir))) {
		char *path = g_build_filename("/sys/class/power_supply", name, "type", NULL), *type = NULL;
		if (g_file_get_contents(path, &type, NULL, NULL) && g_str_has_prefix(type, "Battery")) {
			battery_path = g_path_get_dirname(path); g_free(type); g_free(path); break;
		}
		g_free(type); g_free(path);
	}
	if (dir) g_dir_close(dir);
	refresh(); poll_timer = g_timeout_add_seconds(5, poll, NULL);
}

void xp_tray_stop(void)
{
	stopped = TRUE; tray_canvas = NULL;
	if (poll_timer) g_source_remove(poll_timer);
	if (volume_debounce) g_source_remove(volume_debounce);
	if (volume_click_timer) g_source_remove(volume_click_timer);
	if (brightness_debounce) g_source_remove(brightness_debounce);
	if (audio_event_timer) { g_source_remove(audio_event_timer); audio_event_timer = 0; }
	if (audio_events) g_subprocess_force_exit(audio_events);
	if (tray_menu) { g_signal_handlers_disconnect_by_func(tray_menu, tray_menu_closed, NULL); gtk_widget_destroy(tray_menu); tray_menu = NULL; }
	g_cancellable_cancel(cancellable); g_clear_object(&cancellable);
	g_clear_object(&audio_event_stream); g_clear_object(&audio_events);
	if (volume_window) gtk_widget_destroy(volume_window);
	if (network_window) gtk_widget_destroy(network_window);
	if (date_window) gtk_widget_destroy(date_window);
	if (power_window) gtk_widget_destroy(power_window);
	if (brightness_window) gtk_widget_destroy(brightness_window);
	g_clear_pointer(&brightness_program, g_free); g_clear_pointer(&brightness_device, g_free);
	g_clear_pointer(&audio_program, g_free); g_clear_pointer(&network_program, g_free);
	g_clear_pointer(&traffic, g_hash_table_unref);
	g_clear_pointer(&network_text, g_free); g_clear_pointer(&battery_path, g_free); g_clear_pointer(&battery_text, g_free);
}
