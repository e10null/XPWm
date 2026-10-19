/* XP Display Properties Settings page; native output queries and guarded changes. */
#include "display-settings.h"
#include "xp-style.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	int width, height, refresh;
} DisplayMode;
typedef struct {
	char *name, *description;
	gboolean enabled;
	int width, height, refresh;
	double scale;
	GArray *modes;
} Output;
typedef struct {
	GtkWidget *page, *monitor, *resolution, *rate, *scale, *status, *reset, *confirm, *refresh_button;
	GtkWindow *parent;
	GPtrArray *outputs;
	GSubprocess *query, *guard;
	GDataInputStream *reader;
	gboolean dead, updating, busy, kept, applied, unsaved;
	char *notice;
	guint countdown;
	int seconds;
	void (*changed)(gpointer);
	gpointer changed_data;
	void (*done)(gboolean, gpointer);
	gpointer done_data;
} Settings;

static Settings *get(GtkWidget *page) { return g_object_get_data(G_OBJECT(page), "display-settings"); }
static void load(Settings *s);
static void populate(Settings *s);
static char *helper(void)
{
	char *exe = g_file_read_link("/proc/self/exe", NULL);
	char *dir = exe ? g_path_get_dirname(exe) : g_strdup("shell");
	char *path = g_build_filename(dir, "xpwm-displayctl", NULL);
	g_free(exe);
	g_free(dir);
	return path;
}
static void free_output(gpointer data)
{
	Output *o = data;
	g_free(o->name);
	g_free(o->description);
	g_array_unref(o->modes);
	g_free(o);
}
static Output *selected(Settings *s)
{
	int index = gtk_combo_box_get_active(GTK_COMBO_BOX(s->monitor));
	return index >= 0 && (guint)index < s->outputs->len ? g_ptr_array_index(s->outputs, index) : NULL;
}
static DisplayMode chosen(Settings *s)
{
	DisplayMode mode = {0};
	const char *id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(s->resolution));
	if (id)
		sscanf(id, "%dx%d", &mode.width, &mode.height);
	id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(s->rate));
	if (id)
		mode.refresh = atoi(id);
	return mode;
}
static double chosen_scale(Settings *s)
{
	const char *id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(s->scale));
	return id ? g_ascii_strtod(id, NULL) : 1;
}
gboolean xp_display_settings_dirty(GtkWidget *page)
{
	Settings *s = get(page);
	Output *o = selected(s);
	if (!o || !o->enabled)
		return FALSE;
	DisplayMode m = chosen(s);
	return m.width != o->width || m.height != o->height || m.refresh != o->refresh ||
	       fabs(chosen_scale(s) - o->scale) > .001;
}
gboolean xp_display_settings_busy(GtkWidget *page) { return get(page)->busy; }
static void dirty(Settings *s)
{
	if (s->dead || s->updating)
		return;
	gboolean pending = xp_display_settings_dirty(s->page);
	gtk_widget_set_sensitive(s->monitor, !pending && !s->busy);
	gtk_widget_set_sensitive(s->reset, pending && !s->busy);
	gtk_widget_set_sensitive(s->refresh_button, !pending && !s->busy);
	gtk_label_set_text(GTK_LABEL(s->status), pending
						     ? "Apply or reset this display before selecting another."
						     : "Changes apply to the selected display.");
	if (s->changed)
		s->changed(s->changed_data);
}
static void changed(GtkComboBox *combo, gpointer data) { dirty(data); }
static char *rate_text(int rate)
{
	if (!rate)
		return g_strdup("Default");
	char number[G_ASCII_DTOSTR_BUF_SIZE];
	g_ascii_formatd(number, sizeof(number), "%.3f", rate / 1000.);
	char *end = number + strlen(number) - 1;
	while (end > number && *end == '0')
		*end-- = 0;
	if (*end == '.')
		*end = 0;
	return g_strdup_printf("%s Hertz", number);
}
static void fill_rates(Settings *s)
{
	Output *o = selected(s);
	if (!o)
		return;
	gboolean updating = s->updating;
	s->updating = TRUE;
	DisplayMode m = chosen(s);
	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(s->rate));
	GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	for (guint i = 0; i < o->modes->len; i++) {
		DisplayMode v = g_array_index(o->modes, DisplayMode, i);
		if (v.width != m.width || v.height != m.height)
			continue;
		char id[32];
		g_snprintf(id, sizeof(id), "%d", v.refresh);
		if (g_hash_table_add(seen, g_strdup(id))) {
			char *text = rate_text(v.refresh);
			gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(s->rate), id, text);
			g_free(text);
		}
	}
	g_hash_table_unref(seen);
	char current[32];
	g_snprintf(current, sizeof(current), "%d", o->refresh);
	if (!gtk_combo_box_set_active_id(GTK_COMBO_BOX(s->rate), current))
		gtk_combo_box_set_active(GTK_COMBO_BOX(s->rate), 0);
	s->updating = updating;
}
static void resolution_changed(GtkComboBox *combo, gpointer data)
{
	Settings *s = data;
	if (s->updating)
		return;
	fill_rates(s);
	dirty(s);
}
static void populate(Settings *s)
{
	Output *o = selected(s);
	s->updating = TRUE;
	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(s->resolution));
	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(s->rate));
	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(s->scale));
	if (o) {
		GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
		for (guint i = 0; i < o->modes->len; i++) {
			DisplayMode m = g_array_index(o->modes, DisplayMode, i);
			char *id = g_strdup_printf("%dx%d", m.width, m.height);
			if (g_hash_table_add(seen, g_strdup(id))) {
				char *text = g_strdup_printf("%d by %d pixels", m.width, m.height);
				gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(s->resolution), id, text);
				g_free(text);
			}
			g_free(id);
		}
		g_hash_table_unref(seen);
		char id[64];
		g_snprintf(id, sizeof(id), "%dx%d", o->width, o->height);
		if (!gtk_combo_box_set_active_id(GTK_COMBO_BOX(s->resolution), id))
			gtk_combo_box_set_active(GTK_COMBO_BOX(s->resolution), 0);
		fill_rates(s);
		const double scales[] = {.5, .75, 1, 1.25, 1.5, 1.75, 2, 2.5, 3, 4};
		gboolean found = FALSE;
		for (guint i = 0; i < G_N_ELEMENTS(scales); i++) {
			char value[G_ASCII_DTOSTR_BUF_SIZE];
			g_ascii_dtostr(value, sizeof(value), scales[i]);
			char *text = g_strdup_printf("%g%% %s", scales[i] * 100,
						     scales[i] == 1 ? "(normal size)" : "");
			gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(s->scale), value, text);
			g_free(text);
			if (fabs(scales[i] - o->scale) < .001)
				found = TRUE;
		}
		g_ascii_dtostr(id, sizeof(id), o->scale);
		if (!found) {
			char *text = g_strdup_printf("%.2f%% (current)", o->scale * 100);
			gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(s->scale), id, text);
			g_free(text);
		}
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(s->scale), id);
	}
	gtk_widget_set_sensitive(s->resolution, o && o->enabled);
	gtk_widget_set_sensitive(s->rate, o && o->enabled);
	gtk_widget_set_sensitive(s->scale, o && o->enabled);
	s->updating = FALSE;
	dirty(s);
	if (o && !o->enabled)
		gtk_label_set_text(GTK_LABEL(s->status), "This display is currently disabled.");
}
static void monitor_changed(GtkComboBox *combo, gpointer data)
{
	Settings *s = data;
	if (!s->updating)
		populate(s);
}
static void reset_clicked(GtkButton *button, gpointer data) { populate(data); }
static void refresh_clicked(GtkButton *button, gpointer data)
{
	Settings *s = data;
	if (!s->busy && !xp_display_settings_dirty(s->page))
		load(s);
}
static void list_ready(GObject *source, GAsyncResult *result, gpointer page)
{
	Settings *s = get(page);
	char *output = NULL, *errors = NULL;
	GError *error = NULL;
	gboolean ok =
	    g_subprocess_communicate_utf8_finish(G_SUBPROCESS(source), result, &output, &errors, &error);
	ok = ok && g_subprocess_get_successful(G_SUBPROCESS(source));
	g_clear_object(&s->query);
	if (!s->dead) {
		GKeyFile *ini = g_key_file_new();
		if (ok && output && g_key_file_load_from_data(ini, output, -1, G_KEY_FILE_NONE, NULL)) {
			Output *old = selected(s);
			char *name = old ? g_strdup(old->name) : NULL;
			s->updating = TRUE;
			gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(s->monitor));
			g_ptr_array_set_size(s->outputs, 0);
			int count = CLAMP(g_key_file_get_integer(ini, "outputs", "count", NULL), 0, 128);
			for (int i = 0; i < count; i++) {
				char group[64];
				g_snprintf(group, sizeof(group), "display%d", i);
				Output *o = g_new0(Output, 1);
				o->name = g_key_file_get_string(ini, group, "name", NULL);
				o->description = g_key_file_get_string(ini, group, "description", NULL);
				o->enabled = g_key_file_get_boolean(ini, group, "enabled", NULL);
				o->width = g_key_file_get_integer(ini, group, "width", NULL);
				o->height = g_key_file_get_integer(ini, group, "height", NULL);
				o->refresh = g_key_file_get_integer(ini, group, "refresh", NULL);
				o->scale = g_key_file_get_double(ini, group, "scale", NULL);
				o->modes = g_array_new(FALSE, FALSE, sizeof(DisplayMode));
				int n = CLAMP(g_key_file_get_integer(ini, group, "modes", NULL), 0, 10000);
				for (int j = 0; j < n; j++) {
					char key[32];
					g_snprintf(key, sizeof(key), "mode%d", j);
					char *v = g_key_file_get_string(ini, group, key, NULL);
					DisplayMode m = {0};
					if (v && sscanf(v, "%d %d %d", &m.width, &m.height, &m.refresh) == 3)
						g_array_append_val(o->modes, m);
					g_free(v);
				}
				g_ptr_array_add(s->outputs, o);
				char id[32];
				g_snprintf(id, sizeof(id), "%d", i);
				char *text = g_strdup_printf("%s — %s%s", o->name ? o->name : "Display",
							     o->description ? o->description : "",
							     o->enabled ? "" : " (disabled)");
				gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(s->monitor), id, text);
				g_free(text);
			}
			int index = 0;
			for (guint i = 0; i < s->outputs->len; i++)
				if (name &&
				    !g_strcmp0(name, ((Output *)g_ptr_array_index(s->outputs, i))->name))
					index = i;
			gtk_combo_box_set_active(GTK_COMBO_BOX(s->monitor), s->outputs->len ? index : -1);
			s->updating = FALSE;
			g_free(name);
			populate(s);
			if (!count)
				gtk_label_set_text(GTK_LABEL(s->status), "No displays are available.");
			else if (s->notice)
				gtk_label_set_text(GTK_LABEL(s->status), s->notice);
		} else {
			gtk_label_set_text(GTK_LABEL(s->status),
					   "Display configuration is unavailable. Try Refresh.");
			gtk_widget_set_sensitive(s->resolution, FALSE);
			gtk_widget_set_sensitive(s->rate, FALSE);
			gtk_widget_set_sensitive(s->scale, FALSE);
		}
		g_key_file_unref(ini);
	}
	g_clear_error(&error);
	g_free(output);
	g_free(errors);
	g_object_unref(page);
}
static void load(Settings *s)
{
	if (s->query || s->dead)
		return;
	char *program = helper();
	const char *argv[] = {program, "--list", NULL};
	s->query =
	    g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE, NULL);
	g_free(program);
	if (!s->query) {
		gtk_label_set_text(GTK_LABEL(s->status), "Display configuration is unavailable.");
		return;
	}
	g_subprocess_communicate_utf8_async(s->query, NULL, NULL, list_ready, g_object_ref(s->page));
}

static void stop_countdown(Settings *s)
{
	if (s->countdown) {
		g_source_remove(s->countdown);
		s->countdown = 0;
	}
}
static void answer(Settings *s, gboolean keep)
{
	stop_countdown(s);
	if (s->confirm) {
		GtkWidget *dialog = s->confirm;
		s->confirm = NULL;
		gtk_widget_destroy(dialog);
	}
	if (s->guard) {
		GOutputStream *input = g_subprocess_get_stdin_pipe(s->guard);
		if (!g_output_stream_is_closed(input)) {
			if (keep)
				g_output_stream_write_all(input, "keep\n", 5, NULL, NULL, NULL);
			g_output_stream_close(input, NULL, NULL);
		}
	}
}
static void confirm_response(GtkDialog *dialog, int response, gpointer data)
{
	answer(data, response == GTK_RESPONSE_YES);
}
static gboolean countdown(gpointer data)
{
	Settings *s = data;
	if (s->dead || !s->confirm) {
		s->countdown = 0;
		return G_SOURCE_REMOVE;
	}
	if (--s->seconds <= 0) {
		s->countdown = 0;
		answer(s, FALSE);
		return G_SOURCE_REMOVE;
	}
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(s->confirm),
						 "Your previous settings will be restored in %d seconds.",
						 s->seconds);
	return G_SOURCE_CONTINUE;
}
static void confirm(Settings *s)
{
	if (s->dead)
		return;
	s->confirm = gtk_message_dialog_new(s->parent, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION,
					    GTK_BUTTONS_NONE, "Keep these display settings?");
	gtk_window_set_title(GTK_WINDOW(s->confirm), "Display Settings");
	gtk_dialog_add_buttons(GTK_DIALOG(s->confirm), "Revert", GTK_RESPONSE_NO, "Keep Changes",
			       GTK_RESPONSE_YES, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(s->confirm), GTK_RESPONSE_NO);
	s->seconds = 15;
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(s->confirm),
						 "Your previous settings will be restored in 15 seconds.");
	g_signal_connect(s->confirm, "destroy", G_CALLBACK(gtk_widget_destroyed), &s->confirm);
	g_signal_connect(s->confirm, "response", G_CALLBACK(confirm_response), s);
	xp_style_apply(s->confirm);
	gtk_widget_show_all(s->confirm);
	s->countdown = g_timeout_add_seconds(1, countdown, s);
}
static void read_guard(Settings *s);
static void guard_line(GObject *source, GAsyncResult *result, gpointer page)
{
	Settings *s = get(page);
	GError *error = NULL;
	char *line = g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(source), result, NULL, &error);
	if (!s->dead && line) {
		if (!strcmp(line, "APPLIED")) {
			s->applied = TRUE;
			confirm(s);
		} else if (!strcmp(line, "KEPT"))
			s->kept = TRUE;
		else if (!strcmp(line, "UNSAVED")) {
			s->unsaved = TRUE;
			gtk_label_set_text(
			    GTK_LABEL(s->status),
			    "Settings were applied but could not be saved for the next session.");
		} else if (g_str_has_prefix(line, "ERROR ")) {
			g_free(s->notice);
			s->notice = g_strdup(line + 6);
			gtk_label_set_text(GTK_LABEL(s->status), s->notice);
		} else if (!strcmp(line, "REVERTED")) {
			g_free(s->notice);
			s->notice = g_strdup("The previous display settings were restored.");
			gtk_label_set_text(GTK_LABEL(s->status), s->notice);
		}
	}
	if (line) {
		g_free(line);
		g_clear_error(&error);
		read_guard(s);
		g_object_unref(page);
		return;
	}
	stop_countdown(s);
	if (s->confirm) {
		gtk_widget_destroy(s->confirm);
		s->confirm = NULL;
	}
	g_clear_object(&s->reader);
	g_clear_object(&s->guard);
	s->busy = FALSE;
	if (!s->dead) {
		gtk_widget_set_sensitive(s->page, TRUE);
		if (!s->kept && !s->applied)
			gtk_label_set_text(GTK_LABEL(s->status),
					   "The display change could not be applied. Refresh and try again.");
		if (s->unsaved) {
			GtkWidget *warning = gtk_message_dialog_new(s->parent, GTK_DIALOG_DESTROY_WITH_PARENT,
								    GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
								    "Display settings could not be saved.");
			gtk_message_dialog_format_secondary_text(
			    GTK_MESSAGE_DIALOG(warning), "They are active for this session. Check that your "
							 "configuration directory is writable.");
			g_signal_connect_swapped(warning, "response", G_CALLBACK(gtk_widget_destroy),
						 warning);
			xp_style_apply(warning);
			gtk_widget_show(warning);
		}
		if (s->done)
			s->done(s->kept && !s->unsaved, s->done_data);
		if (!s->dead)
			load(s);
	}
	g_clear_error(&error);
	g_object_unref(page);
}
static void read_guard(Settings *s)
{
	g_data_input_stream_read_line_async(s->reader, G_PRIORITY_DEFAULT, NULL, guard_line,
					    g_object_ref(s->page));
}
void xp_display_settings_apply(GtkWidget *page, void (*done)(gboolean, gpointer), gpointer data)
{
	Settings *s = get(page);
	if (s->busy || s->dead)
		return;
	Output *o = selected(s);
	DisplayMode mode = chosen(s);
	if (!o || !o->enabled || !mode.width) {
		if (done)
			done(FALSE, data);
		return;
	}
	char w[32], h[32], r[32], scale[G_ASCII_DTOSTR_BUF_SIZE];
	g_snprintf(w, sizeof(w), "%d", mode.width);
	g_snprintf(h, sizeof(h), "%d", mode.height);
	g_snprintf(r, sizeof(r), "%d", mode.refresh);
	g_ascii_dtostr(scale, sizeof(scale), chosen_scale(s));
	char *program = helper();
	const char *argv[] = {program, "--preview", o->name, w, h, r, scale, NULL};
	s->guard = g_subprocess_newv(argv,
				     G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE |
					 G_SUBPROCESS_FLAGS_STDERR_PIPE,
				     NULL);
	g_free(program);
	if (!s->guard) {
		gtk_label_set_text(GTK_LABEL(s->status), "Unable to start the display configuration helper.");
		if (done)
			done(FALSE, data);
		return;
	}
	g_clear_pointer(&s->notice, g_free);
	s->busy = TRUE;
	s->kept = s->applied = s->unsaved = FALSE;
	s->done = done;
	s->done_data = data;
	gtk_widget_set_sensitive(s->page, FALSE);
	gtk_label_set_text(GTK_LABEL(s->status), "Testing display settings…");
	s->reader = g_data_input_stream_new(g_subprocess_get_stdout_pipe(s->guard));
	read_guard(s);
}
static void destroyed(GtkWidget *page, gpointer data)
{
	Settings *s = data;
	s->dead = TRUE;
	answer(s, FALSE);
	if (s->query)
		g_subprocess_force_exit(s->query);
}
static void free_settings(gpointer data)
{
	Settings *s = data;
	stop_countdown(s);
	g_clear_object(&s->query);
	g_clear_object(&s->reader);
	g_clear_object(&s->guard);
	g_ptr_array_unref(s->outputs);
	g_free(s->notice);
	g_free(s);
}
static void dropdown_size(GtkComboBox *combo, GtkAllocation *allocation, gpointer data)
{
	AtkObject *accessible = gtk_combo_box_get_popup_accessible(combo);
	GtkWidget *menu = GTK_IS_ACCESSIBLE(accessible) ? gtk_accessible_get_widget(GTK_ACCESSIBLE(accessible)) : NULL;
	if (!GTK_IS_MENU(menu)) return;
	/* GTK ignores popup-fixed-width for grid menus. Match the field explicitly
	 * while retaining its built-in pointer, keyboard and dismissal handling. */
	gtk_widget_set_size_request(menu, allocation->width, -1);
	if (!g_object_get_data(G_OBJECT(menu), "xp-dropdown-styled")) {
		xp_style_apply(menu);
		gtk_style_context_add_class(gtk_widget_get_style_context(menu), "xp-combo-menu");
		g_object_set_data(G_OBJECT(menu), "xp-dropdown-styled", GINT_TO_POINTER(1));
	}
}
static GtkWidget *field(GtkWidget *grid, const char *text, int row)
{
	GtkWidget *label = gtk_label_new_with_mnemonic(text), *combo = gtk_combo_box_text_new();
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), combo);
	gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), combo, 1, row, 1, 1);
	gtk_widget_set_hexpand(combo, TRUE);
	/* One column chooses GTK's below-field anchor rather than positioning
	 * the menu over its selected item. Keep this independent of the host theme. */
	gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(combo), 1);
	gtk_combo_box_set_popup_fixed_width(GTK_COMBO_BOX(combo), TRUE);
	g_signal_connect(combo, "size-allocate", G_CALLBACK(dropdown_size), NULL);
	GList *cells = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(combo));
	for (GList *l = cells; l; l = l->next)
		if (GTK_IS_CELL_RENDERER_TEXT(l->data))
			g_object_set(l->data, "ellipsize", PANGO_ELLIPSIZE_END, "max-width-chars", 32, NULL);
	g_list_free(cells);
	return combo;
}
GtkWidget *xp_display_settings_new(GtkWindow *parent, void (*changed_callback)(gpointer), gpointer data)
{
	Settings *s = g_new0(Settings, 1);
	s->parent = parent;
	s->changed = changed_callback;
	s->changed_data = data;
	s->outputs = g_ptr_array_new_with_free_func(free_output);
	s->page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width(GTK_CONTAINER(s->page), 12);
	g_object_set_data_full(G_OBJECT(s->page), "display-settings", s, free_settings);
	g_signal_connect(s->page, "destroy", G_CALLBACK(destroyed), s);
	GtkWidget *title = gtk_label_new("Select a display to change its screen settings.");
	gtk_label_set_line_wrap(GTK_LABEL(title), TRUE);
	gtk_box_pack_start(GTK_BOX(s->page), title, FALSE, FALSE, 0);
	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
	gtk_box_pack_start(GTK_BOX(s->page), grid, FALSE, FALSE, 0);
	s->monitor = field(grid, "_Display:", 0);
	s->resolution = field(grid, "Screen _resolution:", 1);
	s->rate = field(grid, "_Refresh rate:", 2);
	s->scale = field(grid, "Display _scaling:", 3);
	GtkWidget *hint = gtk_label_new("Scaling changes the size of text, icons and applications.");
	gtk_label_set_line_wrap(GTK_LABEL(hint), TRUE);
	gtk_box_pack_start(GTK_BOX(s->page), hint, FALSE, FALSE, 0);
	GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	s->reset = gtk_button_new_with_label("Reset");
	GtkWidget *refresh = gtk_button_new_with_label("Refresh Displays");
	s->refresh_button = refresh;
	gtk_widget_set_size_request(refresh, 118, 23);
	gtk_box_pack_start(GTK_BOX(buttons), s->reset, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(buttons), refresh, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(s->page), buttons, FALSE, FALSE, 0);
	s->status = gtk_label_new("Checking displays…");
	gtk_label_set_line_wrap(GTK_LABEL(s->status), TRUE);
	gtk_label_set_xalign(GTK_LABEL(s->status), 0);
	gtk_box_pack_start(GTK_BOX(s->page), s->status, FALSE, FALSE, 0);
	g_signal_connect(s->monitor, "changed", G_CALLBACK(monitor_changed), s);
	g_signal_connect(s->resolution, "changed", G_CALLBACK(resolution_changed), s);
	g_signal_connect(s->rate, "changed", G_CALLBACK(changed), s);
	g_signal_connect(s->scale, "changed", G_CALLBACK(changed), s);
	g_signal_connect(s->reset, "clicked", G_CALLBACK(reset_clicked), s);
	g_signal_connect(refresh, "clicked", G_CALLBACK(refresh_clicked), s);
	gtk_widget_set_sensitive(s->reset, FALSE);
	load(s);
	return s->page;
}
