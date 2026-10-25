/* XP-style session action dialog. Actions are explicit and never shell-parsed. */
#include <unistd.h>
#include "session-dialog.h"
#include "session-power.h"
#include "../shared/sounds.h"
#include "../shared/luna.h"
#include "xp-style.h"
#include <gtk-layer-shell.h>

static GtkWidget *dialog, *message;
static GtkWidget *logoff_dialog, *logoff_message;
static GtkWidget *backdrop;

static void show_error(GError *error)
{
	if (!error) return;
	xp_sound_play(XPWM_SOUND_ERROR);
	gtk_label_set_text(GTK_LABEL(message), error->message);
	gtk_widget_set_tooltip_text(message, error->message);
	gtk_widget_show(message);
	g_clear_error(&error);
}

static void power_worker(GTask *task,gpointer source,gpointer data,GCancellable *cancel)
{
 GError *error=NULL;
 if(xp_session_power(GPOINTER_TO_INT(data),&error))g_task_return_boolean(task,TRUE);
 else g_task_return_error(task,error);
}
static void power_done(GObject *source,GAsyncResult *result,gpointer data)
{
 GError *error=NULL;gboolean ok=g_task_propagate_boolean(G_TASK(result),&error);
 if(!g_object_get_data(source,"xp-panel-destroyed")) {
  gtk_widget_set_sensitive(GTK_WIDGET(source),TRUE);
  if(ok)gtk_widget_destroy(GTK_WIDGET(source));
  else if(GTK_WIDGET(source)==dialog){show_error(error);error=NULL;}
 }
 g_clear_error(&error);
}

static gboolean execute_action(int action, GError **error)
{
	const char *name, *argv[4];
	char uid[32];
	if (action == XPWM_SESSION_STANDBY) { name = "suspend";  }
	else if (action == XPWM_SESSION_POWEROFF) { name = "poweroff";  }
	else if (action == XPWM_SESSION_REBOOT) { name = "reboot";  }
	else {
		name = "logoff"; g_snprintf(uid, sizeof(uid), "%u", (unsigned)getuid());
		argv[0] = "loginctl"; argv[1] = "terminate-user"; argv[2] = uid; argv[3] = NULL;
	}
	if (g_getenv("XPWM_SESSION_DRY_RUN")) {
		const char *marker = g_getenv("XPWM_SESSION_MARKER");
		if (marker && !g_file_set_contents(marker, name, -1, error)) return FALSE;
		return TRUE;
	}
	if (action == XPWM_SESSION_POWEROFF || action == XPWM_SESSION_REBOOT)
		xp_sound_play(XPWM_SOUND_SHUTDOWN);
 if(action!=XPWM_SESSION_LOGOFF) {
  GTask *task=g_task_new(dialog,NULL,power_done,NULL);
  g_task_set_task_data(task,GINT_TO_POINTER(action),NULL);
  gtk_widget_set_sensitive(dialog,FALSE);
  g_task_run_in_thread(task,power_worker);g_object_unref(task);return TRUE;
 }
	if (action == XPWM_SESSION_LOGOFF) {
		char *exe = g_file_read_link("/proc/self/exe", NULL);
		char *dir = exe ? g_path_get_dirname(exe) : NULL;
		char *screen = dir ? g_build_filename(dir, "xpwm-logoff", NULL) : NULL;
		const char *screen_argv[2] = {"xpwm-logoff", NULL};
		gboolean result;
		if (screen && g_file_test(screen, G_FILE_TEST_IS_EXECUTABLE)) screen_argv[0] = screen;
		result = g_spawn_async(NULL, (char **)screen_argv, NULL, G_SPAWN_SEARCH_PATH,
			NULL, NULL, NULL, error);
		g_free(screen); g_free(dir); g_free(exe);
		if (result) return TRUE;
		/* An installed shell may omit the helper; retain a safe direct fallback. */
		xp_sound_play(XPWM_SOUND_LOGOFF);
		g_clear_error(error);
	}
	return g_spawn_async(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, error);
}

static void response(GtkDialog *window, gint response_id, gpointer data)
{
	GError *error = NULL;
	if (response_id >= XPWM_SESSION_STANDBY && response_id <= XPWM_SESSION_REBOOT) {
		if (!execute_action(response_id, &error)) show_error(error);
		else if(g_getenv("XPWM_SESSION_DRY_RUN"))gtk_widget_destroy(GTK_WIDGET(window));
	} else gtk_widget_destroy(GTK_WIDGET(window));
}

static void destroyed(GtkWidget *widget, gpointer data)
{
 g_object_set_data(G_OBJECT(widget),"xp-panel-destroyed",GINT_TO_POINTER(1));
	if (backdrop) { gtk_widget_destroy(backdrop); backdrop = NULL; }
	if (widget == dialog) dialog = message = NULL;
	if (widget == logoff_dialog) logoff_dialog = logoff_message = NULL;
}

static void logoff_error(GError *error)
{
	if (!error) return;
	xp_sound_play(XPWM_SOUND_ERROR);
	if (logoff_message) {
		gtk_label_set_text(GTK_LABEL(logoff_message), error->message);
		gtk_widget_set_tooltip_text(logoff_message, error->message);
		gtk_widget_show(logoff_message);
	}
	g_clear_error(&error);
}

static void logoff_response(GtkDialog *window, gint response_id, gpointer data)
{
	GError *error = NULL;
	if (response_id == XPWM_SESSION_LOGOFF) {
		if (!execute_action(XPWM_SESSION_LOGOFF, &error)) { logoff_error(error); return; }
	}
	gtk_widget_destroy(GTK_WIDGET(window));
}

/* The session panels are centered layer surfaces: XP's own artwork supplies
 * their frame, so there is no extra application caption around the panel. */
static gboolean shade_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	cairo_set_source_rgba(cr, 0, 0, 0, .32); cairo_paint(cr);
	return FALSE;
}

static void show_backdrop(void)
{
	backdrop = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_layer_init_for_window(GTK_WINDOW(backdrop));
	gtk_layer_set_namespace(GTK_WINDOW(backdrop), "xpwm-session-shade");
	gtk_layer_set_layer(GTK_WINDOW(backdrop), GTK_LAYER_SHELL_LAYER_OVERLAY);
	gtk_layer_set_exclusive_zone(GTK_WINDOW(backdrop), -1);
	gtk_layer_set_keyboard_mode(GTK_WINDOW(backdrop), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
	for (int edge = 0; edge < GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER; edge++)
		gtk_layer_set_anchor(GTK_WINDOW(backdrop), edge, TRUE);
	gtk_widget_set_app_paintable(backdrop, TRUE);
	gtk_widget_set_visual(backdrop, gdk_screen_get_rgba_visual(gtk_widget_get_screen(backdrop)));
	g_signal_connect(backdrop, "draw", G_CALLBACK(shade_draw), NULL);
	gtk_widget_show(backdrop);
}

static void text(cairo_t *cr, const char *value, int x, int y, const char *font)
{
	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoFontDescription *description = pango_font_description_from_string(font);
	pango_cairo_context_set_resolution(pango_layout_get_context(layout), 96);
	pango_layout_set_font_description(layout, description);
	pango_layout_set_text(layout, value, -1);
	cairo_move_to(cr, x, y); pango_cairo_show_layout(cr, layout);
	pango_font_description_free(description); g_object_unref(layout);
}

static gboolean panel_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	if (!xp_bitmap(cr, "xp-bitmaps/msgina.dll-BITMAP-20142-1033.png", 0, 0)) {
		cairo_pattern_t *gradient = cairo_pattern_create_linear(0, 0, 313, 0);
		cairo_pattern_add_color_stop_rgb(gradient, 0, .32, .45, .80);
		cairo_pattern_add_color_stop_rgb(gradient, .5, .59, .70, .95);
		cairo_pattern_add_color_stop_rgb(gradient, 1, .32, .45, .80);
		cairo_set_source(cr, gradient); cairo_paint(cr); cairo_pattern_destroy(gradient);
		cairo_set_source_rgb(cr, .0, .18, .59);
		cairo_rectangle(cr, 0, 0, 313, 44); cairo_rectangle(cr, 0, 156, 313, 42); cairo_fill(cr);
	}
	if (!xp_bitmap(cr, "xp-bitmaps/msgina.dll-BITMAP-20143-1033.png", 254, 2))
		xp_start_flag(cr, 266, 12);
	cairo_set_source_rgb(cr, 1, 1, 1);
	text(cr, data ? "Log Off Windows" : "Turn off computer", 12, 11, "Tahoma Bold 14");
	return FALSE;
}

static gboolean action_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	int action = GPOINTER_TO_INT(data);
	GtkStateFlags flags = gtk_widget_get_state_flags(widget);
	int state = flags & GTK_STATE_FLAG_ACTIVE ? 1 : flags & GTK_STATE_FLAG_PRELIGHT ? 2 : 0;
	int x = (gtk_widget_get_allocated_width(widget) - 32) / 2;
	const char *strip = action == 0 || action == XPWM_SESSION_LOGOFF ?
		"xp-bitmaps/shell32.dll-BITMAP-14356-1033.png" : "xp-bitmaps/msgina.dll-BITMAP-20150-1033.png";
	int offset = action == XPWM_SESSION_STANDBY ? 3 : action == XPWM_SESSION_REBOOT ? 6 :
		action == XPWM_SESSION_LOGOFF ? 3 : 0;
	cairo_save(cr);
	if (!gtk_widget_is_sensitive(widget)) cairo_push_group(cr);
	if (!xp_bitmap_region(cr, strip, 0, (offset + state) * 32, 32, 32, x, 0)) {
		/* Keep controls recognizable when the user's XP resources are absent. */
		int icon = action == XPWM_SESSION_LOGOFF ? 35 : action == 0 ? 18 : 28;
		if (!xp_icon(cr, icon, 32, x, 0)) {
			cairo_set_source_rgb(cr, action == XPWM_SESSION_STANDBY ? .9 : .8,
				action == XPWM_SESSION_REBOOT ? .7 : .3, .12);
			cairo_rectangle(cr, x, 0, 32, 32); cairo_fill(cr);
			cairo_set_source_rgb(cr, 1, 1, 1); cairo_set_line_width(cr, 2);
			cairo_arc(cr, x+16, 17, 9, -.9, 4.05); cairo_stroke(cr);
			cairo_move_to(cr, x+16, 4); cairo_line_to(cr, x+16, 17); cairo_stroke(cr);
		}
	}
	if (!gtk_widget_is_sensitive(widget)) { cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, .4); }
	cairo_restore(cr);
	return FALSE;
}

static void action_clicked(GtkButton *button, gpointer data)
{
	GtkWidget *panel = gtk_widget_get_toplevel(GTK_WIDGET(button));
	gtk_dialog_response(GTK_DIALOG(panel), GPOINTER_TO_INT(data));
}

static GtkWidget *action_button(GtkWidget *fixed, const char *label, int action, int x, int width)
{
	GtkWidget *button = gtk_button_new_with_mnemonic(label);
	GtkWidget *caption = gtk_bin_get_child(GTK_BIN(button));
	gtk_widget_set_size_request(button, width, 64);
	gtk_widget_set_valign(caption, GTK_ALIGN_END); gtk_widget_set_margin_bottom(caption, 6);
	gtk_style_context_add_class(gtk_widget_get_style_context(button), "xp-icon-button");
	gtk_fixed_put(GTK_FIXED(fixed), button, x, 76);
	g_signal_connect(button, "draw", G_CALLBACK(action_draw), GINT_TO_POINTER(action));
	g_signal_connect(button, "clicked", G_CALLBACK(action_clicked), GINT_TO_POINTER(action));
	if (action == 0) {
		gtk_widget_set_sensitive(button, FALSE);
		gtk_widget_set_tooltip_text(button, "Switch User is not available in this Linux session.");
	}
	return button;
}

static gboolean panel_key(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->keyval != GDK_KEY_Escape) return FALSE;
	gtk_dialog_response(GTK_DIALOG(widget), GTK_RESPONSE_CANCEL);
	return TRUE;
}

static GtkWidget *panel_new(const char *title, gboolean logoff, GtkWidget **error)
{
	static GtkCssProvider *css;
	GtkWidget *panel, *box, *fixed, *background, *cancel, *actions;
	show_backdrop();
	panel = gtk_dialog_new(); gtk_window_set_title(GTK_WINDOW(panel), title);
	gtk_window_set_resizable(GTK_WINDOW(panel), FALSE);
	gtk_layer_init_for_window(GTK_WINDOW(panel));
	gtk_layer_set_namespace(GTK_WINDOW(panel), "xpwm-session-panel");
	gtk_layer_set_layer(GTK_WINDOW(panel), GTK_LAYER_SHELL_LAYER_OVERLAY);
	gtk_layer_set_exclusive_zone(GTK_WINDOW(panel), -1);
	gtk_layer_set_keyboard_mode(GTK_WINDOW(panel), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
	gtk_window_set_default_size(GTK_WINDOW(panel), 313, 198);
	gtk_widget_set_name(panel, "xpwm-session-panel");
	if (!css) {
		css = gtk_css_provider_new();
		gtk_css_provider_load_from_data(css,
			"#xpwm-session-panel, #xpwm-session-panel > box { background:transparent; padding:0; margin:0; border:0; }"
			"#xpwm-session-panel button.xp-icon-button { background:transparent; border:0; min-width:0; min-height:0; padding:0; color:white; }"
			"#xpwm-session-panel button.xp-icon-button:focus { outline:1px dotted white; outline-offset:-2px; }"
			"#xpwm-session-panel button.xp-icon-button:disabled { color:#b5c5ed; }"
			"#xpwm-session-panel .session-error { background:#fff4d2; color:#820000; padding:3px; }", -1, NULL);
		gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(panel), GTK_STYLE_PROVIDER(css),
			GTK_STYLE_PROVIDER_PRIORITY_USER + 3);
	}
	box = gtk_dialog_get_content_area(GTK_DIALOG(panel));
	gtk_container_set_border_width(GTK_CONTAINER(box), 0); gtk_box_set_spacing(GTK_BOX(box), 0);
	fixed = gtk_fixed_new(); gtk_widget_set_size_request(fixed, 313, 198);
	gtk_box_pack_start(GTK_BOX(box), fixed, TRUE, TRUE, 0);
	background = gtk_drawing_area_new(); gtk_widget_set_size_request(background, 313, 198);
	gtk_fixed_put(GTK_FIXED(fixed), background, 0, 0);
	g_signal_connect(background, "draw", G_CALLBACK(panel_draw), GINT_TO_POINTER(logoff));
	if (logoff) {
		action_button(fixed, "_Switch User", 0, 51, 90);
		action_button(fixed, "_Log Off", XPWM_SESSION_LOGOFF, 173, 90);
	} else {
		action_button(fixed, "_Stand By", XPWM_SESSION_STANDBY, 29, 72);
		action_button(fixed, "_Turn Off", XPWM_SESSION_POWEROFF, 120, 72);
		action_button(fixed, "_Restart", XPWM_SESSION_REBOOT, 211, 72);
	}
	cancel = gtk_button_new_with_mnemonic("_Cancel");
	gtk_widget_set_size_request(cancel, 75, 23); gtk_fixed_put(GTK_FIXED(fixed), cancel, 224, 166);
	g_signal_connect(cancel, "clicked", G_CALLBACK(action_clicked), GINT_TO_POINTER(GTK_RESPONSE_CANCEL));
	gtk_widget_set_can_default(cancel, TRUE);
	*error = gtk_label_new(NULL); gtk_label_set_line_wrap(GTK_LABEL(*error), TRUE);
	gtk_label_set_max_width_chars(GTK_LABEL(*error), 44);
	gtk_label_set_lines(GTK_LABEL(*error), 2);
	gtk_label_set_ellipsize(GTK_LABEL(*error), PANGO_ELLIPSIZE_END);
	gtk_widget_set_size_request(*error, 289, -1); gtk_fixed_put(GTK_FIXED(fixed), *error, 12, 45);
	gtk_style_context_add_class(gtk_widget_get_style_context(*error), "session-error");
	gtk_widget_set_no_show_all(*error, TRUE);
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	actions = gtk_dialog_get_action_area(GTK_DIALOG(panel));
	G_GNUC_END_IGNORE_DEPRECATIONS
	gtk_widget_set_no_show_all(actions, TRUE); gtk_widget_hide(actions);
	g_signal_connect(panel, "destroy", G_CALLBACK(destroyed), NULL);
	g_signal_connect(panel, "key-press-event", G_CALLBACK(panel_key), NULL);
	xp_style_apply(panel);
	gtk_widget_show_all(panel); gtk_widget_grab_focus(cancel); gtk_widget_grab_default(cancel);
	return panel;
}

GtkWidget *session_dialog_open(void)
{
	if (dialog) { gtk_window_present(GTK_WINDOW(dialog)); return dialog; }
	if (logoff_dialog) gtk_widget_destroy(logoff_dialog);
	dialog = panel_new("Turn off computer", FALSE, &message);
	g_signal_connect(dialog, "response", G_CALLBACK(response), NULL);
	return dialog;
}

GtkWidget *logoff_dialog_open(void)
{
	if (logoff_dialog) { gtk_window_present(GTK_WINDOW(logoff_dialog)); return logoff_dialog; }
	if (dialog) gtk_widget_destroy(dialog);
	logoff_dialog = panel_new("Log Off Windows", TRUE, &logoff_message);
	g_signal_connect(logoff_dialog, "response", G_CALLBACK(logoff_response), NULL);
	return logoff_dialog;
}
