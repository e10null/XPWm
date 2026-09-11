/* Native dialog prototype. Exact XP dialog units/artwork remain TODO. */
#include "run-dialog.h"
#include "run-command.h"

static GtkWidget *dialog, *entry, *message, *chooser;

static void changed(GtkEditable *editable, gpointer data)
{
	char *value = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK, *g_strstrip(value) != '\0');
	gtk_widget_hide(message);
	g_free(value);
}

static void browse_response(GtkDialog *window, gint response, gpointer data)
{
	if (response == GTK_RESPONSE_ACCEPT) {
		char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(window));
		if (filename) {
			char *quoted = g_shell_quote(filename);
			gtk_entry_set_text(GTK_ENTRY(entry), quoted);
			g_free(quoted); g_free(filename);
		}
	}
	gtk_widget_destroy(GTK_WIDGET(window));
	chooser = NULL;
	gtk_widget_grab_focus(entry);
}

static void response(GtkDialog *window, gint response_id, gpointer data)
{
	GError *error = NULL;
	if (response_id == GTK_RESPONSE_APPLY) {
		if (chooser) { gtk_window_present(GTK_WINDOW(chooser)); return; }
		chooser = gtk_file_chooser_dialog_new("Browse", GTK_WINDOW(dialog), GTK_FILE_CHOOSER_ACTION_OPEN,
			"_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
		gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(chooser), TRUE);
		gtk_window_set_modal(GTK_WINDOW(chooser), TRUE);
		g_signal_connect(chooser, "response", G_CALLBACK(browse_response), NULL);
		gtk_widget_show(chooser);
		return;
	}
	if (response_id == GTK_RESPONSE_OK &&
			!run_command_launch(gtk_entry_get_text(GTK_ENTRY(entry)), &error)) {
		gtk_label_set_text(GTK_LABEL(message), error ? error->message : "Unable to start program.");
		g_clear_error(&error);
		gtk_widget_show(message);
		gtk_widget_grab_focus(entry);
		return;
	}
	gtk_widget_destroy(GTK_WIDGET(window));
}

static void destroyed(GtkWidget *widget, gpointer data)
{
	if (chooser) { gtk_widget_destroy(chooser); chooser = NULL; }
	gtk_style_context_remove_provider_for_screen(gtk_widget_get_screen(widget), GTK_STYLE_PROVIDER(data));
	dialog = entry = message = NULL;
}

GtkWidget *run_dialog_open(void)
{
	GtkWidget *box, *grid, *label;
	GtkCssProvider *css;
	if (dialog) { gtk_window_present(GTK_WINDOW(dialog)); return dialog; }
	dialog = gtk_dialog_new_with_buttons("Run", NULL, 0,
		"_OK", GTK_RESPONSE_OK, "_Cancel", GTK_RESPONSE_CANCEL,
		"_Browse…", GTK_RESPONSE_APPLY, NULL);
	gtk_widget_set_name(dialog, "xpwm-run");
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 380, -1);
	css = gtk_css_provider_new();
	gtk_css_provider_load_from_data(css,
		"#xpwm-run { background:#ece9d8; color:black; font:8pt Tahoma; }"
		"#xpwm-run button { min-height:21px; min-width:65px; padding:0 4px; margin:0; }"
		"#xpwm-run entry { min-height:21px; padding:0 3px; margin:0; }", -1, NULL);
	gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(dialog), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER+1);
	g_object_set_data_full(G_OBJECT(dialog), "run-css", css, g_object_unref);
	g_signal_connect(dialog, "destroy", G_CALLBACK(destroyed), css);
	box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_set_border_width(GTK_CONTAINER(box), 12);
	gtk_box_set_spacing(GTK_BOX(box), 10);
	label = gtk_label_new("Type the name of a program to open.");
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
	grid = gtk_grid_new(); gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	label = gtk_label_new_with_mnemonic("_Open:");
	entry = gtk_entry_new(); gtk_widget_set_hexpand(entry, TRUE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), entry, 1, 0, 1, 1);
	gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 0);
	message = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(message), TRUE);
	gtk_label_set_max_width_chars(GTK_LABEL(message), 50);
	gtk_label_set_xalign(GTK_LABEL(message), 0);
	gtk_widget_set_no_show_all(message, TRUE);
	gtk_box_pack_start(GTK_BOX(box), message, FALSE, FALSE, 0);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK, FALSE);
	g_signal_connect(entry, "changed", G_CALLBACK(changed), NULL);
	g_signal_connect(dialog, "response", G_CALLBACK(response), NULL);
	gtk_widget_show_all(dialog);
	gtk_widget_grab_focus(entry);
	return dialog;
}
