/* XP-style session action dialog. Actions are explicit and never shell-parsed. */
#include <unistd.h>
#include "session-dialog.h"

static GtkWidget *dialog, *message;

static void show_error(GError *error)
{
	if (!error) return;
	gtk_label_set_text(GTK_LABEL(message), error->message);
	gtk_widget_show(message);
	g_clear_error(&error);
}

static gboolean execute_action(int action, GError **error)
{
	const char *name, *argv[4];
	char uid[32];
	if (action == XPWM_SESSION_STANDBY) { name = "suspend"; argv[0] = "systemctl"; argv[1] = "suspend"; argv[2] = NULL; }
	else if (action == XPWM_SESSION_POWEROFF) { name = "poweroff"; argv[0] = "systemctl"; argv[1] = "poweroff"; argv[2] = NULL; }
	else if (action == XPWM_SESSION_REBOOT) { name = "reboot"; argv[0] = "systemctl"; argv[1] = "reboot"; argv[2] = NULL; }
	else {
		name = "logoff"; g_snprintf(uid, sizeof(uid), "%u", (unsigned)getuid());
		argv[0] = "loginctl"; argv[1] = "terminate-user"; argv[2] = uid; argv[3] = NULL;
	}
	if (g_getenv("XPWM_SESSION_DRY_RUN")) {
		const char *marker = g_getenv("XPWM_SESSION_MARKER");
		if (marker && !g_file_set_contents(marker, name, -1, error)) return FALSE;
		return TRUE;
	}
	return g_spawn_async(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, error);
}

static void response(GtkDialog *window, gint response_id, gpointer data)
{
	GError *error = NULL;
	if (response_id >= XPWM_SESSION_STANDBY && response_id <= XPWM_SESSION_LOGOFF) {
		if (!execute_action(response_id, &error)) show_error(error);
		else gtk_widget_destroy(GTK_WIDGET(window));
	} else gtk_widget_destroy(GTK_WIDGET(window));
}

static void destroyed(GtkWidget *widget, gpointer data)
{
	GtkStyleContext *context = gtk_widget_get_style_context(widget);
	gtk_style_context_remove_provider_for_screen(gtk_widget_get_screen(widget), GTK_STYLE_PROVIDER(data));
	dialog = message = NULL; (void)context;
}

GtkWidget *session_dialog_open(void)
{
	GtkWidget *box, *label, *buttons;
	GtkCssProvider *css;
	if (dialog) { gtk_window_present(GTK_WINDOW(dialog)); return dialog; }
	dialog = gtk_dialog_new_with_buttons("Turn off computer", NULL, GTK_DIALOG_MODAL,
		"Stand By", XPWM_SESSION_STANDBY, "Turn Off", XPWM_SESSION_POWEROFF,
		"Restart", XPWM_SESSION_REBOOT, "Cancel", GTK_RESPONSE_CANCEL, NULL);
	gtk_widget_set_name(dialog, "xpwm-session"); gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	css = gtk_css_provider_new();
	gtk_css_provider_load_from_data(css,
		"#xpwm-session { background:#ece9d8; color:#000; font:8pt Tahoma; }"
		"#xpwm-session button { min-height:23px; min-width:72px; padding:0 4px; margin:0 2px; }"
		"#xpwm-session .power { color:#184b9b; font:bold 16pt Tahoma; }"
		"#xpwm-session .error { color:#9b1818; }", -1, NULL);
	gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(dialog), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER+1);
	g_object_set_data_full(G_OBJECT(dialog), "session-css", css, g_object_unref);
	g_signal_connect(dialog, "destroy", G_CALLBACK(destroyed), css);
	box = gtk_dialog_get_content_area(GTK_DIALOG(dialog)); gtk_container_set_border_width(GTK_CONTAINER(box), 12);
	gtk_box_set_spacing(GTK_BOX(box), 10);
	buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	label = gtk_label_new("⏻"); gtk_widget_set_name(label, "power"); gtk_style_context_add_class(gtk_widget_get_style_context(label), "power");
	gtk_box_pack_start(GTK_BOX(buttons), label, FALSE, FALSE, 0);
	label = gtk_label_new("What do you want the computer to do?"); gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_box_pack_start(GTK_BOX(buttons), label, TRUE, TRUE, 0); gtk_box_pack_start(GTK_BOX(box), buttons, FALSE, FALSE, 0);
	message = gtk_label_new(NULL); gtk_widget_set_name(message, "error"); gtk_style_context_add_class(gtk_widget_get_style_context(message), "error");
	gtk_label_set_xalign(GTK_LABEL(message), 0); gtk_label_set_line_wrap(GTK_LABEL(message), TRUE); gtk_widget_set_no_show_all(message, TRUE);
	gtk_box_pack_start(GTK_BOX(box), message, FALSE, FALSE, 0);
	g_signal_connect(dialog, "response", G_CALLBACK(response), NULL);
	gtk_widget_show_all(dialog); gtk_widget_grab_focus(gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL));
	return dialog;
}
