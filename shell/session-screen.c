/* Full-screen XP Welcome/logoff surfaces.  These are visual session helpers:
 * the surrounding display manager/logind remains responsible for real
 * authentication and session accounting. */
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <gtk-layer-shell.h>
#include <cairo.h>
#include <string.h>
#include <unistd.h>

#include "session-screen.h"
#include "../shared/luna.h"
#include "../shared/sounds.h"
#include "session-dialog.h"

static GtkWidget *window, *canvas, *fixed, *password, *login_button, *power_button, *account_button;
static enum XpwmSessionScreenMode screen_mode;
static gboolean selected, finishing, accepting, password_prompt, hovered;
static char *username;
static guint timeout_id;
static guint logoff_delay = 1200;
static gboolean preview_only;
static gboolean logoff_timeout(gpointer data);

static void finish_logon(void);

static void login_activate(GtkWidget *widget, gpointer data)
{
	finish_logon();
}

static void login_clicked(GtkWidget *widget, gpointer data)
{
	finish_logon();
}

static gboolean auto_logon(gpointer data)
{
	timeout_id = 0;
	finish_logon();
	return G_SOURCE_REMOVE;
}

#include "../shared/session-art.h"

static gboolean draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	int width = gtk_widget_get_allocated_width(widget), height = gtk_widget_get_allocated_height(widget);
	if (screen_mode == XPWM_SCREEN_LOGON) {
  XpSessionArt art={username,selected,hovered,accepting,password_prompt,FALSE};
  xp_session_draw_logon(cr, width, height, &art);
 }
	else {
  xp_session_draw_logoff(cr, width, height);
  /* Start the visible dwell time after the first configured frame. */
  if (!timeout_id && !finishing) timeout_id = g_timeout_add(logoff_delay, logoff_timeout, NULL);
 }
	return FALSE;
}

static void place_controls(GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
	int x = xp_session_account_x(allocation->width)+74, y = xp_session_account_y(allocation->height)+68;
	gtk_fixed_move(GTK_FIXED(fixed), password, x, y);
	gtk_fixed_move(GTK_FIXED(fixed), login_button, x+168, y);
	gtk_fixed_move(GTK_FIXED(fixed), power_button, 25, allocation->height-54);
	gtk_fixed_move(GTK_FIXED(fixed), account_button, xp_session_account_x(allocation->width), xp_session_account_y(allocation->height));
}

static void choose_user(void)
{
	if (accepting) return;
	selected = TRUE;
	if (password_prompt) {
		gtk_widget_show(password); gtk_widget_show(login_button);
		gtk_widget_grab_focus(password); gtk_widget_queue_draw(canvas);
	} else finish_logon();
}

static gboolean account_crossing(GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	hovered = event->type == GDK_ENTER_NOTIFY; gtk_widget_queue_draw(canvas); return FALSE;
}

static void account_clicked(GtkWidget *widget, gpointer data) { choose_user(); }

static void write_marker(const char *value)
{
	const char *marker = g_getenv("XPWM_SESSION_MARKER");
	if (marker) g_file_set_contents(marker, value, -1, NULL);
}

static gboolean logon_done(gpointer data)
{
	timeout_id = 0; finishing = TRUE;
	write_marker("PASS: XP logon screen accepted user\n"); gtk_main_quit();
	return G_SOURCE_REMOVE;
}

static void finish_logon(void)
{
	if (finishing || accepting) return;
	accepting = TRUE; xp_sound_play(XPWM_SOUND_LOGON);
	gtk_widget_hide(password); gtk_widget_hide(login_button); gtk_widget_hide(power_button); gtk_widget_hide(account_button);
	gtk_widget_queue_draw(canvas);
	timeout_id = g_timeout_add(450, logon_done, NULL);
}

static void poweroff(GtkWidget *widget, gpointer data) { session_dialog_open(); }

static gboolean go_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	gboolean hot = gtk_widget_get_state_flags(widget) & (GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_ACTIVE);
	if (!xp_bitmap(cr, hot ? "xp-bitmaps/logonui.exe-BITMAP-104-1033.png" :
			"xp-bitmaps/logonui.exe-BITMAP-103-1033.png", 0, 0)) {
		cairo_set_source_rgb(cr, .2, .65, .15); cairo_paint(cr);
		cairo_set_source_rgb(cr, 1, 1, 1); xp_session_text(cr, "→", 2, 0, 24, "Arial Bold 14", PANGO_ALIGN_CENTER);
	}
	return FALSE;
}

static gboolean power_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	if (!xp_bitmap(cr, "xp-bitmaps/logonui.exe-BITMAP-107-1033.png", 0, 0)) {
		cairo_set_source_rgb(cr, .85, .2, .1); cairo_rectangle(cr, 0, 0, 26, 26); cairo_fill(cr);
	}
	cairo_set_source_rgb(cr, 1, 1, 1);
	xp_session_text(cr, "Turn off computer", 32, 2, 215, "Tahoma 14", PANGO_ALIGN_LEFT);
	return FALSE;
}

static gboolean canvas_button(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	int width = gtk_widget_get_allocated_width(widget), height = gtk_widget_get_allocated_height(widget);
	int x = xp_session_account_x(width), y = xp_session_account_y(height);
	if (screen_mode != XPWM_SCREEN_LOGON || event->button != 1 || event->type != GDK_BUTTON_PRESS) return FALSE;
	if (event->x >= x && event->x < x + MIN(308, width-x) && event->y >= y && event->y < y + 80) {
		choose_user(); return TRUE;
	}
	return TRUE;
}

static gboolean key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (screen_mode != XPWM_SCREEN_LOGON) return FALSE;
	if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
		if (!selected) choose_user(); else finish_logon();
		return TRUE;
	}
	if (event->keyval == GDK_KEY_Escape) {
		selected = FALSE; gtk_widget_hide(password); gtk_widget_hide(login_button);
		gtk_widget_grab_focus(canvas); gtk_widget_queue_draw(canvas); return TRUE;
	}
	return FALSE;
}

static gboolean logoff_timeout(gpointer data)
{
	const char *argv[4];
	char uid[32];
	GError *error = NULL;
	timeout_id = 0;
	if (finishing) return G_SOURCE_REMOVE;
	finishing = TRUE;
	if (preview_only || g_getenv("XPWM_SESSION_DRY_RUN")) write_marker("logoff");
	else {
		g_snprintf(uid, sizeof(uid), "%u", (unsigned)getuid());
		argv[0] = "loginctl"; argv[1] = "terminate-user"; argv[2] = uid; argv[3] = NULL;
		if (!g_spawn_async(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH,
			NULL, NULL, NULL, &error)) { g_printerr("XPWm: logoff failed: %s\n", error->message); g_clear_error(&error); }
	}
	gtk_main_quit();
	return G_SOURCE_REMOVE;
}

static void remove_css(GtkWidget *widget, gpointer data)
{
	gtk_style_context_remove_provider_for_screen(gtk_widget_get_screen(widget), GTK_STYLE_PROVIDER(data));
}

int xpwm_session_screen_run(enum XpwmSessionScreenMode mode, int argc, char **argv)
{
	GtkCssProvider *css;
	GtkWidget *overlay;
 GPtrArray *extra_windows = g_ptr_array_new();
	const char *name;
	for (int i=1;i<argc;i++) if (!strcmp(argv[i],"--preview")) preview_only=TRUE;
 if (mode == XPWM_SCREEN_LOGOFF) {
  const char *value=g_getenv("XPWM_LOGOFF_DELAY_MS");
  logoff_delay=preview_only ? 3000 : 1200;
  if(value && *value) logoff_delay=CLAMP((guint)g_ascii_strtoull(value,NULL,10),1,60000);
 }
	screen_mode = mode;
	password_prompt = g_getenv("XPWM_LOGON_PASSWORD_PROMPT") != NULL;
	name = g_get_real_name();
	if (!name || !*name || !g_strcmp0(name, "Unknown")) name = g_get_user_name();
	username = g_strdup(name && *name ? name : "User");
	luna_init();
	gtk_init(&argc, &argv);
	if (!GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default()) || !gtk_layer_is_supported())
		g_error("XPWm session screen requires Wayland and layer-shell");
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_name(window, mode == XPWM_SCREEN_LOGON ? "xpwm-logon" : "xpwm-logoff");
	gtk_layer_init_for_window(GTK_WINDOW(window));
	gtk_layer_set_namespace(GTK_WINDOW(window), mode == XPWM_SCREEN_LOGON ? "xpwm-logon" : "xpwm-logoff");
	gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
	gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
	gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
	gtk_layer_set_exclusive_zone(GTK_WINDOW(window), -1);
	css = gtk_css_provider_new();
	gtk_css_provider_load_from_data(css,
		"#xpwm-logon, #xpwm-logoff { background: transparent; color: white; font: 8pt Tahoma; }"
		"#xpwm-logon entry { min-width:155px; min-height:24px; padding:0 4px; border:1px solid #7392e7; border-radius:3px; background:#fff; color:#000; font:16pt Arial; box-shadow:none; }"
		"#xpwm-logon button { min-width:0; min-height:0; padding:0; border:0; border-image:none; background:transparent; box-shadow:none; outline:none; }", -1, NULL);
	gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(window), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER + 2);
	g_object_set_data_full(G_OBJECT(window), "session-screen-css", css, g_object_unref);
	g_signal_connect(window, "destroy", G_CALLBACK(remove_css), css);
	overlay = gtk_overlay_new(); fixed = gtk_fixed_new(); canvas = gtk_drawing_area_new();
	gtk_widget_set_can_focus(canvas, TRUE); gtk_widget_add_events(canvas, GDK_BUTTON_PRESS_MASK);
	gtk_container_add(GTK_CONTAINER(overlay), canvas); gtk_overlay_add_overlay(GTK_OVERLAY(overlay), fixed);
	gtk_widget_set_halign(fixed, GTK_ALIGN_FILL); gtk_widget_set_valign(fixed, GTK_ALIGN_FILL);
	gtk_container_add(GTK_CONTAINER(window), overlay);
	g_signal_connect(canvas, "draw", G_CALLBACK(draw), NULL);
	g_signal_connect(canvas, "button-press-event", G_CALLBACK(canvas_button), NULL);
	g_signal_connect(window, "key-press-event", G_CALLBACK(key_press), NULL);
	account_button = gtk_button_new(); gtk_widget_set_size_request(account_button, 308, 80);
	atk_object_set_name(gtk_widget_get_accessible(account_button), username);
	gtk_widget_set_tooltip_text(account_button, "Log on");
	g_signal_connect(account_button, "clicked", G_CALLBACK(account_clicked), NULL);
	g_signal_connect(account_button, "enter-notify-event", G_CALLBACK(account_crossing), NULL);
	g_signal_connect(account_button, "leave-notify-event", G_CALLBACK(account_crossing), NULL);
	gtk_fixed_put(GTK_FIXED(fixed), account_button, 0, 0);
	gtk_widget_set_no_show_all(account_button, mode != XPWM_SCREEN_LOGON);
	password = gtk_entry_new(); gtk_entry_set_visibility(GTK_ENTRY(password), FALSE);
	gtk_entry_set_activates_default(GTK_ENTRY(password), TRUE);
	login_button = gtk_button_new(); gtk_widget_set_size_request(login_button, 26, 26);
	gtk_widget_set_tooltip_text(login_button, "Log on");
	atk_object_set_name(gtk_widget_get_accessible(login_button), "Log user on");
	g_signal_connect(login_button, "draw", G_CALLBACK(go_draw), NULL);
	power_button = gtk_button_new(); gtk_widget_set_size_request(power_button, 250, 28);
	gtk_widget_set_tooltip_text(power_button, "Turn off computer");
	atk_object_set_name(gtk_widget_get_accessible(power_button), "Turn off computer");
	g_signal_connect(power_button, "draw", G_CALLBACK(power_draw), NULL);
	g_signal_connect(power_button, "clicked", G_CALLBACK(poweroff), NULL);
	gtk_fixed_put(GTK_FIXED(fixed), power_button, 25, 0);
	gtk_fixed_put(GTK_FIXED(fixed), password, 0, 0); gtk_fixed_put(GTK_FIXED(fixed), login_button, 0, 0);
	g_signal_connect(canvas, "size-allocate", G_CALLBACK(place_controls), NULL);
	gtk_widget_set_no_show_all(power_button, mode != XPWM_SCREEN_LOGON);
	gtk_widget_set_no_show_all(password, TRUE); gtk_widget_set_no_show_all(login_button, TRUE);
	g_signal_connect(password, "activate", G_CALLBACK(login_activate), NULL);
	g_signal_connect(login_button, "clicked", G_CALLBACK(login_clicked), NULL);
	if (mode == XPWM_SCREEN_LOGON) {
		xp_sound_play(XPWM_SOUND_STARTUP);
		if (g_getenv("XPWM_LOGON_AUTO")) timeout_id = g_timeout_add(120, auto_logon, NULL);
	}
	if (mode == XPWM_SCREEN_LOGOFF) {
  GdkDisplay *display=gdk_display_get_default();
  GdkMonitor *primary=gdk_display_get_primary_monitor(display);
  if(!primary) primary=gdk_display_get_monitor(display,0);
  gtk_layer_set_monitor(GTK_WINDOW(window),primary);
  for(int i=0;i<gdk_display_get_n_monitors(display);i++) {
   GdkMonitor *monitor=gdk_display_get_monitor(display,i);
   if(monitor==primary) continue;
   GtkWidget *extra=gtk_window_new(GTK_WINDOW_TOPLEVEL),*view=gtk_drawing_area_new();
   gtk_layer_init_for_window(GTK_WINDOW(extra));gtk_layer_set_namespace(GTK_WINDOW(extra),"xpwm-logoff");
   gtk_layer_set_monitor(GTK_WINDOW(extra),monitor);gtk_layer_set_layer(GTK_WINDOW(extra),GTK_LAYER_SHELL_LAYER_OVERLAY);
   gtk_layer_set_exclusive_zone(GTK_WINDOW(extra),-1);
   for(int edge=0;edge<GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER;edge++)gtk_layer_set_anchor(GTK_WINDOW(extra),edge,TRUE);
   gtk_container_add(GTK_CONTAINER(extra),view);g_signal_connect(view,"draw",G_CALLBACK(draw),NULL);
   gtk_widget_show_all(extra);g_ptr_array_add(extra_windows,extra);
  }
 }
	gtk_widget_show_all(window);
	if (mode == XPWM_SCREEN_LOGOFF) xp_sound_play(XPWM_SOUND_LOGOFF);
	if (mode == XPWM_SCREEN_LOGON) {
		gtk_widget_hide(password); gtk_widget_hide(login_button); gtk_widget_grab_focus(canvas);
	}
	gtk_main();
	if (timeout_id) g_source_remove(timeout_id);
	for(guint i=0;i<extra_windows->len;i++)gtk_widget_destroy(g_ptr_array_index(extra_windows,i));
 g_ptr_array_unref(extra_windows);
	gtk_widget_destroy(window); g_free(username); luna_finish();
	return 0;
}
