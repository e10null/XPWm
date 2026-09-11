/* XP desktop background and icon surface. Native Wayland layer-shell window. */
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <gtk-layer-shell.h>
#include <cairo.h>
#include "../shared/luna.h"

typedef struct {
	const char *label;
	const char *uri;
	const char *kind;
	int x, y;
	gboolean selected;
} DesktopIcon;

static GtkWidget *window, *canvas;
static GdkPixbuf *wallpaper;
static cairo_surface_t *wallpaper_surface;
static char *wallpaper_path;
static int scaled_width, scaled_height;
static GdkPixbuf *scaled_wallpaper;
static DesktopIcon icons[5];
static guint icon_count;
static int wall_mode;
enum { WALL_STRETCH, WALL_CENTER, WALL_TILE };

static void add_wallpaper_candidate(GPtrArray *paths, const char *path)
{
	if (path && g_file_test(path, G_FILE_TEST_IS_REGULAR))
		g_ptr_array_add(paths, g_strdup(path));
}

static GPtrArray *wallpaper_candidates(void)
{
	const char *requested = g_getenv("XPWM_WALLPAPER");
	const char *root = g_getenv("XPWM_ASSET_ROOT");
	const char *relative[] = {
		"assets/local/xp-sp3/sources/I386/BLISS.JP_.expanded/bliss.png",
		"assets/local/xp-sp3/sources/I386/BLISS.JP_.expanded/bliss.jpg",
		"assets/local/xp-sp3/sources/I386/BLISS.JP_.expanded/BLISS.JPG",
	};
	GPtrArray *paths = g_ptr_array_new_with_free_func(g_free);
	char *exe, *exe_dir, *project, *candidate;
	guint i;

	add_wallpaper_candidate(paths, requested);
	for (i = 0; i < G_N_ELEMENTS(relative); i++) add_wallpaper_candidate(paths, relative[i]);
	if (root && *root) {
		for (i = 0; i < G_N_ELEMENTS(relative); i++) {
			candidate = g_build_filename(root, relative[i], NULL);
			add_wallpaper_candidate(paths, candidate); g_free(candidate);
		}
	}
	/* Keep assets discoverable when the shell is started by a display manager
	 * with a different cwd.  The binaries live in <project>/shell/. */
	exe = g_file_read_link("/proc/self/exe", NULL);
	if (exe) {
		exe_dir = g_path_get_dirname(exe);
		project = g_canonicalize_filename("..", exe_dir);
		g_free(exe_dir); g_free(exe);
		for (i = 0; i < G_N_ELEMENTS(relative); i++) {
			candidate = g_build_filename(project, relative[i], NULL);
			add_wallpaper_candidate(paths, candidate); g_free(candidate);
		}
		g_free(project);
	}
	return paths;
}

static void error_dialog(const char *what, GError *error)
{
	GtkWidget *dialog;
	if (!error) return;
	dialog = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", what);
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", error->message);
	g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
	gtk_widget_show(dialog);
	g_error_free(error);
}

static void load_wallpaper(void)
{
	GPtrArray *candidates = wallpaper_candidates();
	GError *error = NULL;
	for (guint i = 0; i < candidates->len; i++) {
		const char *candidate = g_ptr_array_index(candidates, i);
		if (g_str_has_suffix(candidate, ".png") || g_str_has_suffix(candidate, ".PNG")) {
			cairo_surface_t *surface = cairo_image_surface_create_from_png(candidate);
			if (cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS) {
				wallpaper_surface = surface;
				wallpaper_path = g_strdup(candidate);
				break;
			}
			cairo_surface_destroy(surface);
		}
		wallpaper = gdk_pixbuf_new_from_file(candidate, &error);
		if (wallpaper) {
			wallpaper_path = g_strdup(candidate);
			g_clear_error(&error);
			break;
		}
		g_clear_error(&error);
	}
	g_ptr_array_free(candidates, TRUE);
	if (wallpaper || wallpaper_surface) {
		int width = wallpaper_surface ? cairo_image_surface_get_width(wallpaper_surface) : gdk_pixbuf_get_width(wallpaper);
		int height = wallpaper_surface ? cairo_image_surface_get_height(wallpaper_surface) : gdk_pixbuf_get_height(wallpaper);
		g_printerr("XPWm: desktop wallpaper %s (%dx%d)\n", wallpaper_path, width, height);
	}
	else
		g_printerr("XPWm: Bliss wallpaper unavailable; using XP blue fallback\n");
	if (!g_strcmp0(g_getenv("XPWM_WALLPAPER_MODE"), "center")) wall_mode = WALL_CENTER;
	else if (!g_strcmp0(g_getenv("XPWM_WALLPAPER_MODE"), "tile")) wall_mode = WALL_TILE;
	else wall_mode = WALL_STRETCH;
}

static void draw_wallpaper(cairo_t *cr, int width, int height)
{
	int sw, sh, x, y;
	cairo_set_source_rgb(cr, 58 / 255.0, 110 / 255.0, 165 / 255.0);
	cairo_paint(cr);
	if (wallpaper_surface) {
		sw = cairo_image_surface_get_width(wallpaper_surface);
		sh = cairo_image_surface_get_height(wallpaper_surface);
		if (wall_mode == WALL_STRETCH) {
			cairo_save(cr); cairo_scale(cr, (double)width / MAX(1, sw), (double)height / MAX(1, sh));
			cairo_set_source_surface(cr, wallpaper_surface, 0, 0);
			cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR); cairo_paint(cr); cairo_restore(cr);
		} else if (wall_mode == WALL_CENTER) {
			cairo_set_source_surface(cr, wallpaper_surface, (width - sw) / 2, (height - sh) / 2); cairo_paint(cr);
		} else {
			for (y = 0; y < height; y += sh)
				for (x = 0; x < width; x += sw) {
					cairo_set_source_surface(cr, wallpaper_surface, x, y); cairo_paint(cr);
				}
		}
		return;
	}
	if (!wallpaper) return;
	sw = gdk_pixbuf_get_width(wallpaper); sh = gdk_pixbuf_get_height(wallpaper);
	if (wall_mode == WALL_STRETCH) {
		if (!scaled_wallpaper || scaled_width != width || scaled_height != height) {
			g_clear_object(&scaled_wallpaper);
			scaled_wallpaper = gdk_pixbuf_scale_simple(wallpaper, MAX(1, width), MAX(1, height), GDK_INTERP_BILINEAR);
			scaled_width = width; scaled_height = height;
		}
		gdk_cairo_set_source_pixbuf(cr, scaled_wallpaper, 0, 0); cairo_paint(cr);
	} else if (wall_mode == WALL_CENTER) {
		x = (width - sw) / 2; y = (height - sh) / 2;
		gdk_cairo_set_source_pixbuf(cr, wallpaper, x, y); cairo_paint(cr);
	} else {
		for (y = 0; y < height; y += sh)
			for (x = 0; x < width; x += sw) {
				gdk_cairo_set_source_pixbuf(cr, wallpaper, x, y); cairo_paint(cr);
			}
	}
}

static void draw_icon(cairo_t *cr, const DesktopIcon *icon)
{
	int x = icon->x + 20, y = icon->y;
	int resource = !strcmp(icon->kind, "folder") ? 235 : !strcmp(icon->kind, "computer") ? 16 :
		!strcmp(icon->kind, "network") ? 18 : !strcmp(icon->kind, "trash") ? 32 : 512;
	cairo_save(cr);
	/* Pango leaves the current Cairo path untouched. Reset it before an arc or
	 * rectangle so a label's baseline move is never joined to the icon shape. */
	cairo_new_path(cr);
	if (icon->selected) {
		cairo_set_source_rgba(cr, 49 / 255.0, 106 / 255.0, 197 / 255.0, .65);
		cairo_rectangle(cr, icon->x + 3, icon->y - 2, 72, 72); cairo_fill(cr);
	}
	if (xp_icon(cr, resource, 32, x, y)) { cairo_restore(cr); return; }
	cairo_set_line_width(cr, 1);
	if (!strcmp(icon->kind, "folder")) {
		cairo_set_source_rgb(cr, .96, .78, .25); cairo_move_to(cr, x, y + 7); cairo_line_to(cr, x + 10, y + 7);
			cairo_line_to(cr, x + 14, y + 3); cairo_line_to(cr, x + 28, y + 3); cairo_line_to(cr, x + 32, y + 8);
			cairo_line_to(cr, x + 32, y + 29); cairo_line_to(cr, x, y + 29); cairo_close_path(cr); cairo_fill_preserve(cr);
		cairo_set_source_rgb(cr, .55, .35, .05); cairo_stroke(cr);
	} else if (!strcmp(icon->kind, "computer")) {
		cairo_set_source_rgb(cr, .82, .84, .82); cairo_rectangle(cr, x + 1, y + 2, 30, 23); cairo_fill_preserve(cr);
		cairo_set_source_rgb(cr, .25, .30, .34); cairo_stroke(cr);
		cairo_set_source_rgb(cr, .12, .37, .68); cairo_rectangle(cr, x + 4, y + 5, 24, 17); cairo_fill(cr);
		cairo_set_source_rgb(cr, .72, .74, .72); cairo_rectangle(cr, x + 12, y + 25, 8, 3); cairo_fill(cr);
		cairo_rectangle(cr, x + 7, y + 28, 18, 2); cairo_fill(cr);
	} else if (!strcmp(icon->kind, "network")) {
		cairo_set_source_rgb(cr, .32, .72, .93); cairo_arc(cr, x + 16, y + 16, 14, 0, 2 * G_PI); cairo_fill_preserve(cr);
		cairo_set_source_rgb(cr, .05, .25, .55); cairo_stroke(cr);
			cairo_move_to(cr, x + 2, y + 16); cairo_line_to(cr, x + 30, y + 16); cairo_move_to(cr, x + 16, y + 2); cairo_line_to(cr, x + 16, y + 30); cairo_stroke(cr);
	} else if (!strcmp(icon->kind, "trash")) {
		cairo_set_source_rgb(cr, .75, .77, .75); cairo_rectangle(cr, x + 7, y + 7, 18, 24); cairo_fill_preserve(cr);
		cairo_set_source_rgb(cr, .25, .27, .27); cairo_stroke(cr); cairo_rectangle(cr, x + 5, y + 4, 22, 3); cairo_fill(cr);
		cairo_rectangle(cr, x + 12, y + 1, 8, 3); cairo_fill(cr);
	} else {
		cairo_set_source_rgb(cr, .35, .78, .96); cairo_arc(cr, x + 16, y + 16, 15, 0, 2 * G_PI); cairo_fill_preserve(cr);
		cairo_set_source_rgb(cr, .05, .25, .55); cairo_stroke(cr);
		cairo_set_source_rgb(cr, 1, .85, .15); cairo_arc(cr, x + 16, y + 16, 8, -.7, 1.8); cairo_set_line_width(cr, 3); cairo_stroke(cr);
	}
	cairo_restore(cr);
}

static void draw_label(cairo_t *cr, const DesktopIcon *icon)
{
	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoFontDescription *font = pango_font_description_from_string("Tahoma 8");
	pango_cairo_context_set_resolution(pango_layout_get_context(layout), 96);
	pango_layout_set_font_description(layout, font); pango_layout_set_text(layout, icon->label, -1);
	pango_layout_set_width(layout, 76 * PANGO_SCALE); pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
	cairo_new_path(cr);
	cairo_move_to(cr, icon->x + 1, icon->y + 34);
	cairo_set_source_rgb(cr, 0, 0, 0); cairo_save(cr); cairo_translate(cr, 1, 1); pango_cairo_show_layout(cr, layout); cairo_restore(cr);
	cairo_set_source_rgb(cr, 1, 1, 1); pango_cairo_show_layout(cr, layout);
	cairo_new_path(cr);
	pango_font_description_free(font); g_object_unref(layout);
}

static gboolean draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	int width = gtk_widget_get_allocated_width(widget), height = gtk_widget_get_allocated_height(widget);
	draw_wallpaper(cr, width, height);
	for (guint i = 0; i < icon_count; i++) { draw_icon(cr, &icons[i]); draw_label(cr, &icons[i]); }
	return FALSE;
}

static DesktopIcon *hit(double x, double y)
{
	for (guint i = 0; i < icon_count; i++)
		if (x >= icons[i].x && x < icons[i].x + 80 && y >= icons[i].y && y < icons[i].y + 76) return &icons[i];
	return NULL;
}

static void launch(DesktopIcon *icon, guint32 timestamp)
{
	GError *error = NULL;
	if (!gtk_show_uri_on_window(GTK_WINDOW(window), icon->uri, timestamp, &error)) error_dialog("Unable to open desktop item", error);
}

static void clear_selection(void)
{
	for (guint i = 0; i < icon_count; i++) icons[i].selected = FALSE;
}

static void refresh(GtkMenuItem *item, gpointer data) { gtk_widget_queue_draw(canvas); }
static void open_home(GtkMenuItem *item, gpointer data) { launch(&icons[0], gtk_get_current_event_time()); }

static void properties_style_removed(GtkWidget *dialog, gpointer data)
{
	gtk_style_context_remove_provider_for_screen(gtk_widget_get_screen(dialog), GTK_STYLE_PROVIDER(data));
}

static void properties_response(GtkDialog *dialog, gint response_id, gpointer data)
{
	if (response_id == GTK_RESPONSE_OK) {
		GtkWidget *combo = g_object_get_data(G_OBJECT(dialog), "wallpaper-mode");
		int mode = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
		if (mode >= WALL_STRETCH && mode <= WALL_TILE) wall_mode = mode;
		gtk_widget_queue_draw(canvas);
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void properties(GtkMenuItem *item, gpointer data)
{
	GtkWidget *dialog, *box, *row, *label, *combo;
	GtkCssProvider *css = gtk_css_provider_new();
	dialog = gtk_dialog_new_with_buttons("Display Properties", GTK_WINDOW(window),
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		"OK", GTK_RESPONSE_OK, "Cancel", GTK_RESPONSE_CANCEL, NULL);
	gtk_widget_set_name(dialog, "xpwm-properties"); gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_css_provider_load_from_data(css,
		"#xpwm-properties { background:#ece9d8; color:#000; font:8pt Tahoma; }"
		"#xpwm-properties button { min-width:72px; min-height:23px; padding:0 5px; margin:0 2px; }"
		"#xpwm-properties label { color:#000; }", -1, NULL);
	gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(dialog), GTK_STYLE_PROVIDER(css),
		GTK_STYLE_PROVIDER_PRIORITY_USER + 1);
	g_object_set_data_full(G_OBJECT(dialog), "properties-css", css, g_object_unref);
	g_signal_connect(dialog, "destroy", G_CALLBACK(properties_style_removed), css);
	box = gtk_dialog_get_content_area(GTK_DIALOG(dialog)); gtk_container_set_border_width(GTK_CONTAINER(box), 12);
	gtk_box_set_spacing(GTK_BOX(box), 9);
	label = gtk_label_new("Desktop background"); gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
	row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	label = gtk_label_new("Wallpaper position:"); gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(row), label, FALSE, FALSE, 0);
	combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Stretch (XP default)");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Center");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Tile");
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), wall_mode);
	gtk_box_pack_start(GTK_BOX(row), combo, TRUE, TRUE, 0); gtk_box_pack_start(GTK_BOX(box), row, FALSE, FALSE, 0);
	label = gtk_label_new("The selected mode is applied to the desktop immediately.");
	gtk_widget_set_halign(label, GTK_ALIGN_START); gtk_widget_set_opacity(label, .75);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
	g_object_set_data(G_OBJECT(dialog), "wallpaper-mode", combo);
	g_signal_connect(dialog, "response", G_CALLBACK(properties_response), NULL);
	gtk_widget_show_all(dialog);
}

static GtkWidget *desktop_menu_new(void)
{
	GtkWidget *menu = gtk_menu_new(), *item;
	item = gtk_menu_item_new_with_label("Refresh");
	g_signal_connect(item, "activate", G_CALLBACK(refresh), NULL); gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	item = gtk_menu_item_new_with_label("Open Home folder");
	g_signal_connect(item, "activate", G_CALLBACK(open_home), NULL); gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
	item = gtk_menu_item_new_with_label("Properties");
	g_signal_connect(item, "activate", G_CALLBACK(properties), NULL); gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show_all(menu);
	return menu;
}

static gboolean button(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	DesktopIcon *icon = hit(event->x, event->y);
	if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
		GtkWidget *menu = desktop_menu_new();
		gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event); return TRUE;
	}
	if (event->button != 1) return FALSE;
	if (event->type == GDK_BUTTON_PRESS) {
		clear_selection(); if (icon) icon->selected = TRUE; gtk_widget_queue_draw(canvas);
	} else if (event->type == GDK_2BUTTON_PRESS && icon) launch(icon, event->time);
	return TRUE;
}

static gboolean test_ready(gpointer data)
{
	const char *marker = g_getenv("XPWM_DESKTOP_TEST");
	const char *screenshot = g_getenv("XPWM_DESKTOP_SCREENSHOT");
	if (!marker) return G_SOURCE_REMOVE;
	g_assert(icon_count == 5);
	g_assert(wallpaper != NULL || wallpaper_surface != NULL || g_getenv("XPWM_ALLOW_NO_WALLPAPER"));
	{
		GtkWidget *menu = desktop_menu_new();
		GList *items = gtk_container_get_children(GTK_CONTAINER(menu));
		g_assert_cmpuint(g_list_length(items), ==, 4);
		g_assert_cmpstr(gtk_menu_item_get_label(GTK_MENU_ITEM(g_list_last(items)->data)), ==, "Properties");
		g_list_free(items); gtk_widget_destroy(menu);
	}
	if (screenshot) {
		char *quoted = g_shell_quote(screenshot);
		char *command = g_strdup_printf("grim %s", quoted);
		g_assert(g_spawn_command_line_sync(command, NULL, NULL, NULL, NULL));
		g_free(command); g_free(quoted);
	}
	g_assert(g_file_set_contents(marker, "PASS: desktop wallpaper, XP icon column and layer surface\n", -1, NULL));
	gtk_main_quit(); return G_SOURCE_REMOVE;
}

static void init_icons(void)
{
	const char *home = g_get_home_dir();
	const char *documents = g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS);
	icons[0] = (DesktopIcon){"My Documents", documents ? documents : home, "folder", 16, 18, FALSE};
	icons[1] = (DesktopIcon){"My Computer", "computer:///", "computer", 16, 98, FALSE};
	icons[2] = (DesktopIcon){"My Network Places", "network:///", "network", 16, 178, FALSE};
	icons[3] = (DesktopIcon){"Recycle Bin", "trash:///", "trash", 16, 258, FALSE};
	icons[4] = (DesktopIcon){"Internet Explorer", "https://www.microsoft.com/", "browser", 16, 338, FALSE};
	icon_count = G_N_ELEMENTS(icons);
}

int main(int argc, char **argv)
{
	luna_init();
	g_setenv("GTK_THEME", "Adwaita", TRUE);
	gtk_init(&argc, &argv); load_wallpaper(); init_icons();
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_layer_init_for_window(GTK_WINDOW(window));
	gtk_layer_set_namespace(GTK_WINDOW(window), "xpwm-desktop"); gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_BACKGROUND);
	gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
	gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
	gtk_layer_set_exclusive_zone(GTK_WINDOW(window), 0);
	canvas = gtk_drawing_area_new(); gtk_widget_add_events(canvas, GDK_BUTTON_PRESS_MASK);
	gtk_container_add(GTK_CONTAINER(window), canvas); g_signal_connect(canvas, "draw", G_CALLBACK(draw), NULL);
	g_signal_connect(canvas, "button-press-event", G_CALLBACK(button), NULL);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	gtk_widget_show_all(window);
	if (g_getenv("XPWM_DESKTOP_TEST")) g_idle_add(test_ready, NULL);
	gtk_main();
	g_clear_object(&scaled_wallpaper); g_clear_object(&wallpaper);
	if (wallpaper_surface) cairo_surface_destroy(wallpaper_surface);
	g_free(wallpaper_path); return 0;
}
