/* Resource-backed Luna Start menu with categorized application folders. */
#include "start-menu.h"
#include <gio/gdesktopappinfo.h>
#include "xp-style.h"
#include "../shared/luna.h"
#include "run-dialog.h"
#include "search-dialog.h"
#include "session-dialog.h"
#include "../shared/sounds.h"

enum { PROGRAM_TOP_GAP = 8 };

static void open_run(GtkMenuItem *item, gpointer data) { run_dialog_open(); }
static void open_session(GtkMenuItem *item, gpointer data) { session_dialog_open(); }
static void open_logoff(GtkMenuItem *item, gpointer data) { logoff_dialog_open(); }
static void report_error(GError *error);

static void open_settings(GtkMenuItem *item, gpointer data)
{
	const char *candidates[] = {"systemsettings5", "gnome-control-center", "xfce4-settings-manager"};
	for (guint i = 0; i < G_N_ELEMENTS(candidates); i++) {
		char *program = g_find_program_in_path(candidates[i]);
		if (program) {
			GError *error = NULL;
			if (!g_spawn_command_line_async(program, &error)) report_error(error);
			g_free(program);
			return;
		}
	}
	run_dialog_open();
}

static void open_help(GtkMenuItem *item, gpointer data)
{
	GError *error = NULL;
	gtk_show_uri_on_window(NULL, "https://support.microsoft.com", gtk_get_current_event_time(), &error);
	report_error(error);
}

static void open_search(GtkMenuItem *item, gpointer data) { search_dialog_open(); }

static void report_error(GError *error)
{
	GtkWidget *dialog;
	if (!error) return;
	xp_sound_play(XPWM_SOUND_ERROR);
	dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR,
		GTK_BUTTONS_CLOSE, "Unable to open application");
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", error->message);
	g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
	gtk_widget_show(dialog);
	g_error_free(error);
}

static void launch(GtkMenuItem *item, gpointer data)
{
	GError *error = NULL;
	GdkAppLaunchContext *context = gdk_display_get_app_launch_context(gdk_display_get_default());
	gdk_app_launch_context_set_timestamp(context, gtk_get_current_event_time());
	/* GIO expands desktop-entry field codes; never pass Exec through a shell. */
	g_app_info_launch(G_APP_INFO(data), NULL, G_APP_LAUNCH_CONTEXT(context), &error);
	g_object_unref(context);
	report_error(error);
}

static void home(GtkMenuItem *item, gpointer data)
{
	GError *error = NULL;
	char *uri = g_filename_to_uri(data ? data : g_get_home_dir(), NULL, &error);
	if (uri) gtk_show_uri_on_window(NULL, uri, gtk_get_current_event_time(), &error);
	g_free(uri);
	report_error(error);
}

static void open_uri(GtkMenuItem *item, gpointer data)
{
 GError *error = NULL;
 gtk_show_uri_on_window(NULL, data, gtk_get_current_event_time(), &error);
 report_error(error);
}
static void open_printers(GtkMenuItem *item, gpointer data)
{
 const char *candidates[] = {"system-config-printer", "print-manager"};
 for (guint i = 0; i < G_N_ELEMENTS(candidates); i++) {
  char *program = g_find_program_in_path(candidates[i]);
  if (program) {
   char *argv[] = {program, NULL}; GError *error = NULL;
   g_spawn_async(NULL, argv, NULL, 0, NULL, NULL, NULL, &error);
   g_free(program); report_error(error); return;
  }
 }
 open_uri(item, "http://localhost:631/printers/");
}
static gint recent_order(gconstpointer a, gconstpointer b)
{
 time_t left = gtk_recent_info_get_modified((GtkRecentInfo *)a);
 time_t right = gtk_recent_info_get_modified((GtkRecentInfo *)b);
 return left == right ? 0 : left > right ? -1 : 1;
}
static GtkWidget *recent_documents(void)
{
 GtkWidget *menu = gtk_menu_new();
 GList *items = gtk_recent_manager_get_items(gtk_recent_manager_get_default());
 items = g_list_sort(items, recent_order); guint count = 0;
 for (GList *l = items; l && count < 15; l = l->next) {
  GtkRecentInfo *info = l->data;
  if (!gtk_recent_info_is_local(info) || gtk_recent_info_get_private_hint(info) || !gtk_recent_info_exists(info)) continue;
  const char *uri = gtk_recent_info_get_uri(info);
  GtkWidget *item = gtk_menu_item_new_with_label(gtk_recent_info_get_display_name(info));
  GtkWidget *text = gtk_bin_get_child(GTK_BIN(item));
  gtk_label_set_ellipsize(GTK_LABEL(text), PANGO_ELLIPSIZE_END); gtk_label_set_max_width_chars(GTK_LABEL(text), 36);
  char *display_uri = gtk_recent_info_get_uri_display(info); gtk_widget_set_tooltip_text(item, display_uri); g_free(display_uri);
  g_object_set_data_full(G_OBJECT(item), "recent-uri", g_strdup(uri), g_free);
  g_signal_connect(item, "activate", G_CALLBACK(open_uri), g_object_get_data(G_OBJECT(item), "recent-uri"));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item); count++;
 }
 g_list_free_full(items, (GDestroyNotify)gtk_recent_info_unref);
 if (!count) {
  GtkWidget *empty = gtk_menu_item_new_with_label("(Empty)"); gtk_widget_set_sensitive(empty, FALSE);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), empty);
 }
 xp_style_apply(menu); gtk_widget_show_all(menu); return menu;
}

static gint compare_apps(gconstpointer a, gconstpointer b)
{
	return g_utf8_collate(g_app_info_get_display_name(G_APP_INFO((gpointer)a)),
		g_app_info_get_display_name(G_APP_INFO((gpointer)b)));
}

static gboolean has_category(GAppInfo *app, const char *category)
{
	const char *categories = G_IS_DESKTOP_APP_INFO(app) ?
		g_desktop_app_info_get_categories(G_DESKTOP_APP_INFO(app)) : NULL;
	if (!categories) return FALSE;
	for (const char *p = categories; *p;) {
		const char *end = strchr(p, ';');
		size_t length = end ? (size_t)(end-p) : strlen(p);
		if (strlen(category) == length && !strncmp(p, category, length)) return TRUE;
		if (!end) break;
		p = end+1;
	}
	return FALSE;
}

/* Specific roles win over broad categories, independent of metadata order.
 * Each launcher belongs to exactly one folder, including multi-category apps. */
static const char *app_folder(GAppInfo *app)
{
	if (has_category(app, "Settings")) return "Settings";
	if (has_category(app, "Game")) return "Games";
	if (has_category(app, "Development")) return "Development";
	if (has_category(app, "Office")) return "Office";
	if (has_category(app, "Graphics")) return "Graphics";
	if (has_category(app, "AudioVideo") || has_category(app, "Audio") || has_category(app, "Video")) return "Entertainment";
	if (has_category(app, "Network")) return "Internet";
	if (has_category(app, "Education") || has_category(app, "Science")) return "Education";
	if (has_category(app, "TerminalEmulator") || has_category(app, "FileManager") ||
			has_category(app, "Utility")) return "Accessories";
	if (has_category(app, "System")) return "System Tools";
	return "Other Programs";
}

static void bind_app(GtkWidget *item, GAppInfo *app)
{
	g_object_set_data_full(G_OBJECT(item), "application", g_object_ref(app), g_object_unref);
	g_signal_connect(item, "activate", G_CALLBACK(launch), app);
	const char *description = g_app_info_get_description(app);
	if (description && *description) gtk_widget_set_tooltip_text(item, description);
}

enum MenuGlyph { GLYPH_USER, GLYPH_APP, GLYPH_FOLDER, GLYPH_LIST, GLYPH_RUN, GLYPH_POWER };

/* Small deterministic glyphs keep the Start menu usable on systems without a
 * freedesktop icon theme. They are deliberately drawn in the same style as
 * the desktop fallback icons; later resource mapping can replace them without
 * changing menu layout or activation. */
static void glyph(cairo_t *cr, enum MenuGlyph kind, int x, int y)
{
	cairo_save(cr); cairo_new_path(cr); cairo_set_line_width(cr, 1);
	if (kind == GLYPH_FOLDER) {
		cairo_set_source_rgb(cr, .97, .78, .24); cairo_move_to(cr, x, y + 8);
		cairo_line_to(cr, x + 9, y + 8); cairo_line_to(cr, x + 13, y + 4);
		cairo_line_to(cr, x + 25, y + 4); cairo_line_to(cr, x + 29, y + 9);
		cairo_line_to(cr, x + 29, y + 27); cairo_line_to(cr, x, y + 27); cairo_close_path(cr);
		cairo_fill_preserve(cr); cairo_set_source_rgb(cr, .48, .30, .04); cairo_stroke(cr);
	} else if (kind == GLYPH_USER) {
		cairo_set_source_rgb(cr, .93, .78, .55); cairo_arc(cr, x + 16, y + 9, 6, 0, 2 * G_PI); cairo_fill(cr);
		cairo_set_source_rgb(cr, .26, .52, .87); cairo_arc(cr, x + 16, y + 25, 11, G_PI, 2 * G_PI); cairo_fill(cr);
	} else if (kind == GLYPH_LIST) {
		cairo_set_source_rgb(cr, .25, .57, .91); cairo_rectangle(cr, x + 3, y + 3, 26, 26); cairo_fill_preserve(cr);
		cairo_set_source_rgb(cr, .08, .25, .59); cairo_stroke(cr); cairo_set_source_rgb(cr, 1, 1, 1);
		for (int i = 0; i < 3; i++) { cairo_rectangle(cr, x + 8, y + 8 + i * 6, 3, 3); cairo_fill(cr); cairo_rectangle(cr, x + 14, y + 8 + i * 6, 10, 2); cairo_fill(cr); }
	} else if (kind == GLYPH_RUN) {
		cairo_set_source_rgb(cr, .78, .82, .86); cairo_rectangle(cr, x + 3, y + 5, 26, 20); cairo_fill_preserve(cr);
		cairo_set_source_rgb(cr, .15, .22, .31); cairo_stroke(cr); cairo_set_source_rgb(cr, .08, .33, .72);
		cairo_rectangle(cr, x + 6, y + 8, 20, 14); cairo_fill(cr);
	} else if (kind == GLYPH_POWER) {
		cairo_set_source_rgb(cr, .83, .20, .14); cairo_arc(cr, x + 16, y + 16, 12, -.75, 3.9); cairo_stroke(cr);
		cairo_set_source_rgb(cr, .83, .20, .14); cairo_set_line_width(cr, 3); cairo_move_to(cr, x + 16, y + 3); cairo_line_to(cr, x + 16, y + 16); cairo_stroke(cr);
	} else {
		cairo_set_source_rgb(cr, .86, .91, .98); cairo_rectangle(cr, x + 3, y + 4, 26, 22); cairo_fill_preserve(cr);
		cairo_set_source_rgb(cr, .19, .31, .48); cairo_stroke(cr); cairo_set_source_rgb(cr, .15, .45, .84);
		cairo_rectangle(cr, x + 7, y + 8, 18, 13); cairo_fill(cr);
	}
	cairo_restore(cr);
}

static gboolean background(GtkWidget *menu, cairo_t *cr, gpointer data)
{
	int w = gtk_widget_get_allocated_width(menu), h = gtk_widget_get_allocated_height(menu);
	int half = w / 2, body = MAX(1, h - 104);
	cairo_set_source_rgb(cr, .12, .44, .85); cairo_paint(cr);
	cairo_set_source_rgb(cr, 1, 1, 1); cairo_rectangle(cr, 0, 64, half, body); cairo_fill(cr);
	cairo_set_source_rgb(cr, .83, .90, .98); cairo_rectangle(cr, half, 64, w-half, body); cairo_fill(cr);
	luna_draw(cr, "start-header", 0, 0, 0, w, 64);
	luna_draw(cr, "start-programs", 0, 0, 64, half, MAX(1, body-30));
	luna_draw(cr, "start-more", 0, 0, 64+body-30, half, 30);
	luna_draw(cr, "start-places", 0, half, 64, w-half, body);
	luna_draw(cr, "start-footer", 0, 0, h-40, w, 40);
	cairo_set_source_rgb(cr, .64, .73, .84);
	cairo_rectangle(cr, half+10, 64+PROGRAM_TOP_GAP+5*36-1, half-20, 1);
	cairo_rectangle(cr, half+10, 64+PROGRAM_TOP_GAP+8*36-1, half-20, 1); cairo_fill(cr);
	if (GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(menu), "pin-count"))) {
		int pins = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(menu), "pin-count"));
		cairo_set_source_rgb(cr, .83, .83, .81);
		cairo_rectangle(cr, 10, 64+PROGRAM_TOP_GAP+pins*36-1, half-20, 1); cairo_fill(cr);
	}
	/* Explorer's menu has a one-pixel blue perimeter; GTK's native menu
	 * shadow is disabled by the provider above, so keep this edge explicit. */
	cairo_set_source_rgba(cr, 0.03, 0.18, 0.52, .9);
	cairo_set_line_width(cr, 1); cairo_rectangle(cr, .5, .5, w-1, h-1); cairo_stroke(cr);
	return FALSE;
}

static void remove_style(GtkWidget *menu, gpointer data)
{
	gtk_style_context_remove_provider_for_screen(gtk_widget_get_screen(menu), GTK_STYLE_PROVIDER(data));
}

static gboolean navigation(GtkWidget *menu, GdkEventKey *event, gpointer data)
{
	if (data) menu = data;
	GtkWidget *selected, *target = NULL;
	GList *items, *l;
	int column = 0, best = event->keyval == GDK_KEY_Home ? G_MAXINT : -1;
	GtkWidget *active = menu;
	selected = gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(active));
	while (selected && gtk_menu_item_get_submenu(GTK_MENU_ITEM(selected)) &&
			gtk_widget_get_mapped(gtk_menu_item_get_submenu(GTK_MENU_ITEM(selected)))) {
		active = gtk_menu_item_get_submenu(GTK_MENU_ITEM(selected));
		selected = gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(active));
	}
	/* Opening a submenu need not select its first item. Keep Right inside the
	 * deepest popup instead of letting GTK's grid navigation jump columns. */
	if (event->keyval == GDK_KEY_Right && active != menu && !selected) {
		gtk_menu_shell_select_first(GTK_MENU_SHELL(active), TRUE);
		return TRUE;
	}
	if (active != menu &&
			(event->keyval == GDK_KEY_Home || event->keyval == GDK_KEY_End ||
			 event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_Down ||
			 event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)) {
		GtkWidget *submenu = active;
		GtkWidget *current = gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(submenu));
		GList *enabled = NULL;
		int index, count;
		items = gtk_container_get_children(GTK_CONTAINER(submenu));
		for (l = items; l; l = l->next)
			if (gtk_widget_get_sensitive(l->data) && gtk_widget_get_visible(l->data))
				enabled = g_list_append(enabled, l->data);
		g_list_free(items);
		count = g_list_length(enabled);
		index = g_list_index(enabled, current);
		if (count) {
			if (event->keyval == GDK_KEY_Home) index = 0;
			else if (event->keyval == GDK_KEY_End) index = count-1;
			else if (event->keyval == GDK_KEY_Down) index = MIN(index+1, count-1);
			else if (event->keyval == GDK_KEY_Up) index = index < 0 ? count-1 : MAX(0, index-1);
			target = g_list_nth_data(enabled, MAX(0, index));
		}
		g_list_free(enabled);
		if (target) {
			gtk_menu_shell_select_item(GTK_MENU_SHELL(submenu), target);
			if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
				if (gtk_menu_item_get_submenu(GTK_MENU_ITEM(target))) gtk_widget_activate(target);
				else gtk_menu_shell_activate_item(GTK_MENU_SHELL(submenu), target, TRUE);
			}
		}
		return TRUE;
	}
	if (event->keyval == GDK_KEY_Left && active != menu) {
		gtk_menu_popdown(GTK_MENU(active));
		return TRUE;
	}
	/* GtkMenu's grid traversal otherwise moves Right into the adjacent column
	 * even when the selected item owns a submenu. */
	if (event->keyval == GDK_KEY_Right && selected &&
			gtk_menu_item_get_submenu(GTK_MENU_ITEM(selected))) {
		gtk_widget_activate(selected);
		return TRUE;
	}
	if (event->keyval != GDK_KEY_Home && event->keyval != GDK_KEY_End) return FALSE;
	if (selected) gtk_container_child_get(GTK_CONTAINER(menu), selected, "left-attach", &column, NULL);
	items = gtk_container_get_children(GTK_CONTAINER(menu));
	for (l = items; l; l = l->next) {
		int left, row;
		GtkWidget *item = l->data;
		if (!gtk_widget_get_sensitive(item) || !gtk_widget_get_visible(item)) continue;
		gtk_container_child_get(GTK_CONTAINER(menu), item, "left-attach", &left, "top-attach", &row, NULL);
		/* Footer buttons are a separate action band in XP. Home/End select
		 * program/place entries, not the session controls. */
		if (left == column && row < 12 && (event->keyval == GDK_KEY_Home ? row < best : row > best)) {
			target = item; best = row;
		}
	}
	g_list_free(items);
	if (target) gtk_menu_shell_select_item(GTK_MENU_SHELL(menu), target);
	return TRUE;
}

static gboolean item_icon(GtkWidget *item, cairo_t *cr, gpointer data)
{
	int resource = GPOINTER_TO_INT(data);
	if (resource == -1) {
		int w = gtk_widget_get_allocated_width(item), h = gtk_widget_get_allocated_height(item);
		cairo_set_source_rgb(cr, .15, .55, .13);
		cairo_move_to(cr, w-21, h/2-7); cairo_line_to(cr, w-13, h/2);
		cairo_line_to(cr, w-21, h/2+7); cairo_close_path(cr); cairo_fill(cr);
		return FALSE;
	}
	int height = gtk_widget_get_allocated_height(item);
	int size = height < 34 ? 16 : 32;
	GAppInfo *app = g_object_get_data(G_OBJECT(item), "application");
	GIcon *icon = app ? g_app_info_get_icon(app) : NULL;
	if (resource == 3 && icon) {
		GtkIconInfo *info = gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_for_screen(
			gtk_widget_get_screen(item)), icon, size, GTK_ICON_LOOKUP_FORCE_SIZE);
		GdkPixbuf *pixbuf = info ? gtk_icon_info_load_icon(info, NULL) : NULL;
		if (info) g_object_unref(info);
		if (pixbuf) {
			gdk_cairo_set_source_pixbuf(cr, pixbuf, size == 16 ? 3 : 7, (height-size)/2);
			cairo_paint(cr);
			g_object_unref(pixbuf);
			return FALSE;
		}
	}
	if (resource == -114) {
		cairo_set_source_rgb(cr, .93, .96, 1); cairo_rectangle(cr, 6.5, (height-48)/2-.5, 51, 49);
		cairo_set_line_width(cr, 2); cairo_stroke(cr);
	}
	int drawn = resource == -114 ?
		xp_bitmap(cr, "xp-bitmaps/logonui.exe-BITMAP-114-1033.png", 8, (height - 48) / 2) :
		xp_icon(cr, resource, size, size == 16 ? 3 : 7, (height-size)/2);
	if (!drawn)
		glyph(cr, resource == -114 ? GLYPH_USER : GLYPH_APP, 7, (height-32)/2);
	return FALSE;
}

static GtkWidget *cell(GtkWidget *menu, const char *label, int left, int right,
		int row, int height, const char *style)
{
	GtkWidget *item = gtk_menu_item_new_with_label(label);
	GtkWidget *text = gtk_bin_get_child(GTK_BIN(item));
	gtk_label_set_ellipsize(GTK_LABEL(text), PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars(GTK_LABEL(text), right-left == 2 ? 40 : 22);
	gtk_widget_set_size_request(item, (right-left)*190, height);
	if (row == 1) gtk_widget_set_margin_top(item, PROGRAM_TOP_GAP);
	if (style) gtk_style_context_add_class(gtk_widget_get_style_context(item), style);
	gtk_menu_attach(GTK_MENU(menu), item, left, right, row, row+1);
	if (*label) {
		int resource = 3;
		if (row == 0) resource = -114;
		else if (!strcmp(label, "Internet")) resource = 512;
		else if (!strcmp(label, "E-mail")) resource = 172;
		else if (!strcmp(label, "My Documents")) resource = 235;
		else if (!strcmp(label, "My Recent Documents")) resource = 21;
		else if (!strcmp(label, "My Pictures")) resource = 236;
		else if (!strcmp(label, "My Music")) resource = 237;
		else if (!strcmp(label, "My Computer")) resource = 16;
		else if (!strcmp(label, "My Network Places")) resource = 18;
		else if (!strcmp(label, "Control Panel")) resource = 20;
		else if (!strcmp(label, "Printers and Faxes")) resource = 17;
		else if (!strcmp(label, "Help and Support")) resource = 24;
		else if (!strcmp(label, "Search")) resource = 23;
		else if (row == 11) resource = left ? 25 : -1;
		else if (row == 12) resource = left ? 28 : 35;
		g_signal_connect_after(item, "draw", G_CALLBACK(item_icon), GINT_TO_POINTER(resource));
	}
	return item;
}

/* Pin only defaults present in this session's visible application list. This
 * also keeps private XDG test/managed application directories authoritative. */
static GAppInfo *visible_default(GList *apps, const char *scheme)
{
	GAppInfo *preferred = g_app_info_get_default_for_uri_scheme(scheme), *match = NULL;
	for (GList *l = apps; preferred && l; l = l->next)
		if (g_app_info_should_show(l->data) && g_app_info_equal(preferred, l->data)) {
			match = l->data; break;
		}
	if (preferred) g_object_unref(preferred);
	return match;
}

GtkWidget *start_menu_new(void)
{
	GtkWidget *menu = gtk_menu_new(), *programs = gtk_menu_new(), *item;
	GList *apps = g_list_sort(g_app_info_get_all(), compare_apps), *l;
	unsigned count = 0, shortcuts = 0;
	const char *folders[] = {"Accessories", "Development", "Education", "Entertainment",
		"Games", "Graphics", "Internet", "Office", "Other Programs", "Settings", "System Tools"};
	GtkWidget *submenus[G_N_ELEMENTS(folders)] = {0};
	GAppInfo *pinned[] = {visible_default(apps, "https"), visible_default(apps, "mailto")};
	const char *pin_names[] = {"Internet", "E-mail"};
	const char *username = g_get_real_name();
	GtkCssProvider *css = gtk_css_provider_new();
	const GUserDirectory directories[] = {G_USER_DIRECTORY_PICTURES, G_USER_DIRECTORY_MUSIC};
	const char *names[] = {"My Pictures", "My Music"};
	gtk_widget_set_name(menu, "xpwm-start");
	gtk_css_provider_load_from_data(css,
		"#xpwm-start { padding:0; margin:0; border:0; background:transparent; box-shadow:none; }"
		"#xpwm-start menuitem { padding:0 8px 0 43px; margin:0; border:0; min-height:0; min-width:0; background:transparent; color:#000; font:8pt Tahoma; }"
		"#xpwm-start menuitem:hover { background:#316ac5; color:white; }"
		"#xpwm-start .header { color:white; padding-left:64px; font:bold 11pt Tahoma; }"
		"#xpwm-start .header:disabled, #xpwm-start .header:disabled label { color:white; opacity:1; }"
		"#xpwm-start .footer { color:white; }"
		"#xpwm-start .programs { padding-left:42px; padding-right:34px; font-weight:bold; }"
		"#xpwm-start .personal, #xpwm-start .pin-title { font-weight:bold; }"
		"#xpwm-start .pin-detail { color:#777; }"
		"#xpwm-start menuitem:hover .pin-detail { color:white; }", -1, NULL);
	gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(menu), GTK_STYLE_PROVIDER(css),
		GTK_STYLE_PROVIDER_PRIORITY_USER + 1);
	/* Provider is removed when the menu dies; it only targets this menu's ID. */
	g_object_set_data_full(G_OBJECT(menu), "start-css", css, g_object_unref);
	g_signal_connect(menu, "destroy", G_CALLBACK(remove_style), css);
	g_signal_connect(menu, "draw", G_CALLBACK(background), NULL);
	g_signal_connect(menu, "key-press-event", G_CALLBACK(navigation), NULL);
	g_signal_connect(programs, "key-press-event", G_CALLBACK(navigation), menu);
	if (!username || !*username || !strcmp(username, "Unknown")) username = g_get_user_name();
	item = cell(menu, username, 0, 2, 0, 64, "header");
	gtk_widget_set_sensitive(item, FALSE);
	item = cell(menu, "My Documents", 1, 2, 1, 36, "personal");
	g_signal_connect(item, "activate", G_CALLBACK(home), NULL);
	item = cell(menu, "My Recent Documents", 1, 2, 2, 36, NULL);
 GtkWidget *recent = recent_documents();
 gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), recent);
 g_signal_connect(recent, "key-press-event", G_CALLBACK(navigation), menu);
	for (unsigned i = 0; i < G_N_ELEMENTS(directories); i++) {
		const char *path = g_get_user_special_dir(directories[i]);
		item = cell(menu, names[i], 1, 2, i+3, 36, "personal");
		if (path && g_file_test(path, G_FILE_TEST_IS_DIR)) {
			g_object_set_data_full(G_OBJECT(item), "folder", g_strdup(path), g_free);
			g_signal_connect(item, "activate", G_CALLBACK(home), g_object_get_data(G_OBJECT(item), "folder"));
		} else gtk_widget_set_sensitive(item, FALSE);
	}
	item = cell(menu, "My Computer", 1, 2, 5, 36, "personal");
	g_signal_connect(item, "activate", G_CALLBACK(open_uri), "computer:///");
	item = cell(menu, "My Network Places", 1, 2, 6, 36, NULL);
	g_signal_connect(item, "activate", G_CALLBACK(open_uri), "network:///");
	item = cell(menu, "Printers and Faxes", 1, 2, 7, 36, NULL);
	g_signal_connect(item, "activate", G_CALLBACK(open_printers), NULL);
	item = cell(menu, "Control Panel", 1, 2, 8, 36, NULL);
	g_signal_connect(item, "activate", G_CALLBACK(open_settings), NULL);
	item = cell(menu, "Help and Support", 1, 2, 9, 36, NULL);
	g_signal_connect(item, "activate", G_CALLBACK(open_help), NULL);
	item = cell(menu, "Search", 1, 2, 10, 36, NULL);
	g_signal_connect(item, "activate", G_CALLBACK(open_search), NULL);
	item = cell(menu, "All Programs", 0, 1, 11, 30, "programs");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), programs);
	g_object_set_data(G_OBJECT(menu), "programs", programs);
	item = cell(menu, "Run…", 1, 2, 11, 30, NULL);
	g_signal_connect(item, "activate", G_CALLBACK(open_run), NULL);
	item = cell(menu, "Log Off", 0, 1, 12, 40, "footer");
	g_signal_connect(item, "activate", G_CALLBACK(open_logoff), NULL);
	item = cell(menu, "Turn Off Computer...", 1, 2, 12, 40, "footer");
	g_signal_connect(item, "activate", G_CALLBACK(open_session), NULL);
	for (guint i = 0; i < G_N_ELEMENTS(pinned); i++) {
		if (!pinned[i] || (i && pinned[i] == pinned[0])) continue;
		item = cell(menu, pin_names[i], 0, 1, ++shortcuts, 36, "pinned");
		GtkWidget *labels = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		GtkWidget *primary = gtk_label_new(pin_names[i]);
		GtkWidget *secondary = gtk_label_new(g_app_info_get_display_name(pinned[i]));
		gtk_widget_destroy(gtk_bin_get_child(GTK_BIN(item)));
		gtk_label_set_xalign(GTK_LABEL(primary), 0); gtk_label_set_xalign(GTK_LABEL(secondary), 0);
		gtk_label_set_ellipsize(GTK_LABEL(secondary), PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars(GTK_LABEL(secondary), 22);
		gtk_style_context_add_class(gtk_widget_get_style_context(primary), "pin-title");
		gtk_style_context_add_class(gtk_widget_get_style_context(secondary), "pin-detail");
		gtk_box_pack_start(GTK_BOX(labels), primary, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(labels), secondary, FALSE, FALSE, 0);
		gtk_container_add(GTK_CONTAINER(item), labels);
		gtk_menu_reorder_child(GTK_MENU(menu), item, shortcuts);
		g_object_set_data_full(G_OBJECT(item), "application", g_object_ref(pinned[i]), g_object_unref);
		g_signal_connect(item, "activate", G_CALLBACK(launch), pinned[i]);
	}
	g_object_set_data(G_OBJECT(menu), "pin-count", GUINT_TO_POINTER(shortcuts));
	for (l = apps; l; l = l->next) {
		GAppInfo *app = l->data;
		if (!g_app_info_should_show(app)) continue;
		const char *folder = app_folder(app);
		for (guint i = 0; i < G_N_ELEMENTS(folders); i++) {
			if (strcmp(folder, folders[i])) continue;
			if (!submenus[i]) submenus[i] = gtk_menu_new();
			item = gtk_menu_item_new_with_label(g_app_info_get_display_name(app));
			GtkWidget *label = gtk_bin_get_child(GTK_BIN(item));
			gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
			gtk_label_set_max_width_chars(GTK_LABEL(label), 36);
			bind_app(item, app);
			g_signal_connect_after(item, "draw", G_CALLBACK(item_icon), GINT_TO_POINTER(3));
			gtk_menu_shell_append(GTK_MENU_SHELL(submenus[i]), item);
			break;
		}
		count++;
	}
	for (guint i = 0; i < G_N_ELEMENTS(folders); i++) {
		if (!submenus[i]) continue;
		item = gtk_menu_item_new_with_label(folders[i]);
		g_signal_connect_after(item, "draw", G_CALLBACK(item_icon), GINT_TO_POINTER(4));
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenus[i]);
		gtk_menu_shell_append(GTK_MENU_SHELL(programs), item);
		xp_style_apply(submenus[i]);
		g_signal_connect(submenus[i], "key-press-event", G_CALLBACK(navigation), menu);
		gtk_widget_show_all(submenus[i]);
	}
	/* The front page is a short set of useful roles, not the first six names
	 * in the installed-app list. Browser and mail remain pinned above it. */
	const char *roles[] = {"FileManager", "TextEditor", "Calculator", "Player"};
	GList *favorites = NULL;
	for (guint i = 0; i < G_N_ELEMENTS(roles); i++) {
		for (l = apps; l; l = l->next) {
			GAppInfo *app = l->data;
			if (!g_app_info_should_show(app) || app == pinned[0] || app == pinned[1] ||
				g_list_find(favorites, app) || !has_category(app, roles[i]) ||
				has_category(app, "ConsoleOnly") || has_category(app, "Settings")) continue;
			item = cell(menu, g_app_info_get_display_name(app), 0, 1, ++shortcuts, 36, NULL);
			gtk_menu_reorder_child(GTK_MENU(menu), item, shortcuts);
			bind_app(item, app);
			favorites = g_list_prepend(favorites, app);
			break;
		}
	}
	g_list_free(favorites);
	g_list_free_full(apps, g_object_unref);
	while (shortcuts < 10) {
		item = cell(menu, "", 0, 1, ++shortcuts, 36, NULL);
		gtk_menu_reorder_child(GTK_MENU(menu), item, shortcuts);
		gtk_widget_set_sensitive(item, FALSE);
	}
	if (!count) {
		item = gtk_menu_item_new_with_label("No applications installed");
		gtk_widget_set_sensitive(item, FALSE);
		gtk_menu_shell_append(GTK_MENU_SHELL(programs), item);
	}
	xp_style_apply(programs);
	gtk_widget_show_all(programs);
	gtk_widget_show_all(menu);
	return menu;
}
