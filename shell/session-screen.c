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

static void text(cairo_t *cr, const char *value, double x, double y, double width,
		const char *font, PangoAlignment alignment)
{
	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoFontDescription *description = pango_font_description_from_string(font);
	pango_cairo_context_set_resolution(pango_layout_get_context(layout), 96);
	pango_layout_set_font_description(layout, description);
	pango_layout_set_text(layout, value ? value : "", -1);
	pango_layout_set_width(layout, MAX(1, (int)width) * PANGO_SCALE);
	pango_layout_set_alignment(layout, alignment);
	pango_layout_set_single_paragraph_mode(layout, TRUE);
	cairo_new_path(cr); cairo_move_to(cr, x, y); pango_cairo_show_layout(cr, layout);
	pango_font_description_free(description); g_object_unref(layout);
}

static void xp_logo(cairo_t *cr, double x, double y, double scale)
{
	static const double colors[4][3] = {
		{.88, .12, .10}, {.15, .70, .18}, {.10, .35, .85}, {.96, .72, .08}
	};
	cairo_save(cr); cairo_translate(cr, x, y); cairo_scale(cr, scale, scale);
	for (int i = 0; i < 4; i++) {
		double ox = (i & 1) * 18, oy = (i / 2) * 18;
		cairo_set_source_rgb(cr, colors[i][0], colors[i][1], colors[i][2]);
		cairo_move_to(cr, ox, oy + (i / 2 ? 1 : 0));
		cairo_line_to(cr, ox + 16, oy); cairo_line_to(cr, ox + 16, oy + 16);
		cairo_line_to(cr, ox, oy + (i / 2 ? 15 : 16)); cairo_close_path(cr); cairo_fill(cr);
	}
	cairo_restore(cr);
}

/* Geometry and colors follow LogonUI's UIFILE 1000, not the Luna desktop.
 * Top band: 80rp. Footer: 20rp padding + two instruction lines + divider. */
static int content_middle(int height) { return (80 + height - 74) / 2; }
static int account_x(int width) { return width / 2 + 26; }
static int account_y(int height) { return content_middle(height) - 40; }

static void background(cairo_t *cr, int width, int height)
{
	cairo_pattern_t *gradient;
	cairo_set_source_rgb(cr, 90/255., 126/255., 220/255.); cairo_paint(cr);
	cairo_save(cr); cairo_rectangle(cr, 0, 80, width, MAX(1, height-154)); cairo_clip(cr);
	xp_bitmap(cr, "xp-bitmaps/logonui.exe-BITMAP-100-1033.png", 0, 80);
	cairo_restore(cr);
	cairo_set_source_rgb(cr, 0, 48/255., 156/255.);
	cairo_rectangle(cr, 0, 0, width, 80); cairo_fill(cr);
	gradient = cairo_pattern_create_linear(0, 0, width, 0);
	cairo_pattern_add_color_stop_rgb(gradient, 0, 57/255., 52/255., 173/255.);
	cairo_pattern_add_color_stop_rgb(gradient, 1, 0, 48/255., 156/255.);
	cairo_set_source(cr, gradient); cairo_rectangle(cr, 0, height-74, width, 74); cairo_fill(cr);
	cairo_pattern_destroy(gradient);
	cairo_save(cr); cairo_scale(cr, width/800., 1);
	xp_bitmap(cr, "xp-bitmaps/logonui.exe-BITMAP-125-1033.png", 0, 78);
	xp_bitmap(cr, "xp-bitmaps/logonui.exe-BITMAP-126-1033.png", 0, height-74);
	cairo_restore(cr);
}

static void fallback_user(cairo_t *cr, double x, double y)
{
	cairo_set_source_rgb(cr, .78, .86, .96); cairo_arc(cr, x + 24, y + 17, 9, 0, 2 * G_PI); cairo_fill(cr);
	cairo_set_source_rgb(cr, .35, .55, .78); cairo_arc(cr, x + 24, y + 43, 18, G_PI, 2 * G_PI); cairo_fill(cr);
}

static void product(cairo_t *cr, double x, double y)
{
	if (xp_bitmap(cr, "xp-bitmaps/logonui.exe-BITMAP-123-1033.png", x, y)) return;
	xp_logo(cr, x+53, y, 1.6);
	cairo_set_source_rgb(cr, 1, 1, 1);
	text(cr, "Microsoft", x, y+52, 137, "Arial 7", PANGO_ALIGN_LEFT);
	text(cr, "Windows", x, y+61, 137, "Arial Bold 20", PANGO_ALIGN_LEFT);
}

static void user_tile(cairo_t *cr, int width, int height)
{
	int x = account_x(width), y = account_y(height);
	if (selected) xp_bitmap(cr, "xp-bitmaps/logonui.exe-BITMAP-112-1033.png", x, y);
	if (!xp_bitmap(cr, (selected || hovered) ? "xp-bitmaps/logonui.exe-BITMAP-119-1033.png" :
			"xp-bitmaps/logonui.exe-BITMAP-113-1033.png", x+7, y+7)) {
		cairo_set_source_rgb(cr, 1, 1, .9); cairo_rectangle(cr, x+7, y+7, 58, 58); cairo_fill(cr);
	}
	if (!xp_bitmap(cr, "xp-bitmaps/logonui.exe-BITMAP-114-1033.png", x+12, y+12))
		fallback_user(cr, x+12, y+12);
	cairo_set_source_rgb(cr, 239/255., 247/255., 1);
	text(cr, username, x+72, y+18, MAX(1, width-x-90), "Tahoma 14", PANGO_ALIGN_LEFT);
	if (selected && password_prompt)
		text(cr, "Type your password", x+74, y+49, 230, "Arial 8", PANGO_ALIGN_LEFT);
}

static void draw_logon(cairo_t *cr, int width, int height)
{
	int middle = content_middle(height), split = width/2;
	background(cr, width, height);
	if (accepting) {
		cairo_set_source_rgb(cr, 49/255., 81/255., 181/255.);
		text(cr, "welcome", split-264, middle-30+3, 244, "Arial Bold Italic 36", PANGO_ALIGN_RIGHT);
		cairo_set_source_rgb(cr, 239/255., 247/255., 1);
		text(cr, "welcome", split-266, middle-30, 244, "Arial Bold Italic 36", PANGO_ALIGN_RIGHT);
		return;
	}
	product(cr, split-157, middle-70);
	cairo_set_source_rgb(cr, 239/255., 247/255., 1);
	text(cr, "To begin, click your user name", MAX(0, split-384), middle+36,
		MIN(344, split-16), "Arial 14", PANGO_ALIGN_RIGHT);
	if (!xp_bitmap(cr, "xp-bitmaps/logonui.exe-BITMAP-124-1033.png", split, middle-170)) {
		cairo_set_source_rgba(cr, 1, 1, 1, .3); cairo_rectangle(cr, split, middle-170, 1, 340); cairo_fill(cr);
	}
	user_tile(cr, width, height);
	cairo_set_source_rgb(cr, 1, 1, 1);
	if (width >= 720) {
		text(cr, "After you log on, you can add or change accounts.", width-350, height-52,
			325, "Tahoma 9", PANGO_ALIGN_LEFT);
		text(cr, "Just go to Control Panel and click User Accounts.", width-350, height-37,
			325, "Tahoma 9", PANGO_ALIGN_LEFT);
	}
}

static void draw_logoff(cairo_t *cr, int width, int height)
{
	int middle = content_middle(height);
	background(cr, width, height);
	product(cr, width/2-69, middle-77);
	cairo_set_source_rgb(cr, 239/255., 247/255., 1);
	text(cr, "Logging off...", width/2-190, middle+30, 380, "Tahoma 10", PANGO_ALIGN_CENTER);
}

static gboolean draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	int width = gtk_widget_get_allocated_width(widget), height = gtk_widget_get_allocated_height(widget);
	if (screen_mode == XPWM_SCREEN_LOGON) draw_logon(cr, width, height);
	else draw_logoff(cr, width, height);
	return FALSE;
}

static void place_controls(GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
	int x = account_x(allocation->width)+74, y = account_y(allocation->height)+68;
	gtk_fixed_move(GTK_FIXED(fixed), password, x, y);
	gtk_fixed_move(GTK_FIXED(fixed), login_button, x+168, y);
	gtk_fixed_move(GTK_FIXED(fixed), power_button, 25, allocation->height-54);
	gtk_fixed_move(GTK_FIXED(fixed), account_button, account_x(allocation->width), account_y(allocation->height));
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
		cairo_set_source_rgb(cr, 1, 1, 1); text(cr, "→", 2, 0, 24, "Arial Bold 14", PANGO_ALIGN_CENTER);
	}
	return FALSE;
}

static gboolean power_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	if (!xp_bitmap(cr, "xp-bitmaps/logonui.exe-BITMAP-107-1033.png", 0, 0)) {
		cairo_set_source_rgb(cr, .85, .2, .1); cairo_rectangle(cr, 0, 0, 26, 26); cairo_fill(cr);
	}
	cairo_set_source_rgb(cr, 1, 1, 1);
	text(cr, "Turn off computer", 32, 2, 215, "Tahoma 14", PANGO_ALIGN_LEFT);
	return FALSE;
}

static gboolean canvas_button(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	int width = gtk_widget_get_allocated_width(widget), height = gtk_widget_get_allocated_height(widget);
	int x = account_x(width), y = account_y(height);
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
	if (g_getenv("XPWM_SESSION_DRY_RUN")) write_marker("logoff");
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
	const char *name;
	(void)argc; (void)argv;
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
	gtk_layer_set_keyboard_mode(GTK_WINDOW(window), mode == XPWM_SCREEN_LOGON ?
		GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE : GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
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
	} else {
		guint delay = 1200; const char *value = g_getenv("XPWM_LOGOFF_DELAY_MS");
		if (value && *value) delay = CLAMP((guint)g_ascii_strtoull(value, NULL, 10), 1, 60000);
		timeout_id = g_timeout_add(delay, logoff_timeout, NULL);
	}
	gtk_widget_show_all(window);
	if (mode == XPWM_SCREEN_LOGOFF) xp_sound_play(XPWM_SOUND_LOGOFF);
	if (mode == XPWM_SCREEN_LOGON) {
		gtk_widget_hide(password); gtk_widget_hide(login_button); gtk_widget_grab_focus(canvas);
	}
	gtk_main();
	if (timeout_id) g_source_remove(timeout_id);
	gtk_widget_destroy(window); g_free(username); luna_finish();
	return 0;
}
