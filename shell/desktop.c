/* XP desktop background and icon surface. Native Wayland layer-shell window. */
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <gtk-layer-shell.h>
#include <cairo.h>
#include <math.h>
#include "desktop-files.h"
#include "../shared/luna.h"
#include "xp-style.h"
#include "display-settings.h"
#include "../shared/sounds.h"

typedef struct {
	char *label;
	char *uri;
	const char *kind;
	int x, y;
	gboolean selected;
 GFile *file; GdkPixbuf *pixbuf; guint64 size, modified; gboolean positioned;
 cairo_surface_t *rendered; gboolean rendered_selected; double rendered_scale_x, rendered_scale_y;
} DesktopIcon;

static GtkWidget *window, *canvas;
static GdkPixbuf *wallpaper;
static cairo_surface_t *wallpaper_surface;
static char *wallpaper_path;
static cairo_surface_t *wallpaper_cache;
static int cached_width, cached_height, cached_wall_mode = -1;
static double cached_scale_x, cached_scale_y;
static DesktopIcon *icons;
static gboolean desktop_dead, pointer_down, rubber_select, dragging;
static double press_x, press_y, rubber_x, rubber_y;
static int *drag_start, focused_icon, selection_anchor;
static gboolean plain_click, rubber_toggle, rubber_add;
static gboolean *selection_base;
static void reload_icons(void);
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
	xp_style_apply(dialog); gtk_widget_show(dialog);
	xp_sound_play(XPWM_SOUND_ERROR);
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

static void draw_wallpaper_source(cairo_t *cr, int width, int height)
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
  cairo_save(cr); cairo_scale(cr, (double)width/MAX(1, sw), (double)height/MAX(1, sh));
  gdk_cairo_set_source_pixbuf(cr, wallpaper, 0, 0);
  cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR); cairo_paint(cr); cairo_restore(cr);
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

static void draw_wallpaper(cairo_t *cr, int width, int height)
{
 double sx, sy;
 if (width <= 0 || height <= 0) return;
 cairo_surface_get_device_scale(cairo_get_target(cr), &sx, &sy);
 if (!wallpaper_cache || cached_width != width || cached_height != height || cached_wall_mode != wall_mode || cached_scale_x != sx || cached_scale_y != sy) {
  if (wallpaper_cache) cairo_surface_destroy(wallpaper_cache);
  wallpaper_cache = cairo_image_surface_create(CAIRO_FORMAT_RGB24, ceil(width*sx), ceil(height*sy));
  cairo_surface_set_device_scale(wallpaper_cache, sx, sy);
  cairo_t *background = cairo_create(wallpaper_cache);
  draw_wallpaper_source(background, width, height); cairo_destroy(background);
  cached_width = width; cached_height = height; cached_wall_mode = wall_mode; cached_scale_x = sx; cached_scale_y = sy;
 }
 cairo_set_source_surface(cr, wallpaper_cache, 0, 0); cairo_paint(cr);
}

static void draw_icon(cairo_t *cr, const DesktopIcon *icon)
{
	int x = icon->x + 20, y = icon->y;
	int resource = icon->file ? (!strcmp(icon->kind, "folder") ? 4 : 1) : !strcmp(icon->kind, "folder") ? 235 : !strcmp(icon->kind, "computer") ? 16 :
		!strcmp(icon->kind, "network") ? 18 : !strcmp(icon->kind, "trash") ? 32 : 512;
	cairo_save(cr);
	/* Pango leaves the current Cairo path untouched. Reset it before an arc or
	 * rectangle so a label's baseline move is never joined to the icon shape. */
	cairo_new_path(cr);
 /* Keep selection tint buffers local to this icon, even on a full redraw. */
 cairo_rectangle(cr, x-1, y-1, 34, 34); cairo_clip(cr);
	if (icon->selected) cairo_push_group(cr);
	gboolean drawn = FALSE;
 if (icon->pixbuf) { gdk_cairo_set_source_pixbuf(cr, icon->pixbuf, x, y); cairo_paint(cr); drawn = TRUE; }
 else drawn = xp_icon(cr, resource, 32, x, y);
 if (drawn) {
		if (icon->selected) {
			cairo_set_operator(cr, CAIRO_OPERATOR_ATOP);
			cairo_set_source_rgba(cr, 49 / 255.0, 106 / 255.0, 197 / 255.0, .5);
			cairo_paint(cr); cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
			cairo_pop_group_to_source(cr); cairo_paint(cr);
		}
		cairo_restore(cr); return;
	}
	if (icon->selected) { cairo_pop_group_to_source(cr); cairo_paint(cr); }
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
 pango_layout_set_height(layout, -2); pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	int text_height;
	pango_layout_get_pixel_size(layout, NULL, &text_height);
	cairo_new_path(cr);
	if (icon->selected) {
		PangoRectangle ink;
		pango_layout_get_pixel_extents(layout, &ink, NULL);
		cairo_set_source_rgb(cr, 49 / 255.0, 106 / 255.0, 197 / 255.0);
		cairo_rectangle(cr, icon->x + ink.x - 1, icon->y + 34, ink.width + 4, text_height + 1);
		cairo_fill_preserve(cr);
		cairo_set_source_rgb(cr, 1, 1, 1);
		cairo_set_line_width(cr, 1);
		{ double dash[] = {1, 1}; cairo_set_dash(cr, dash, 2, 0); cairo_stroke(cr); cairo_set_dash(cr, NULL, 0, 0); }
	} else {
		cairo_move_to(cr, icon->x + 2, icon->y + 35);
		cairo_set_source_rgb(cr, 0, 0, 0); pango_cairo_show_layout(cr, layout);
	}
	cairo_move_to(cr, icon->x + 1, icon->y + 34);
	cairo_set_source_rgb(cr, 1, 1, 1); pango_cairo_show_layout(cr, layout);
	cairo_new_path(cr);
	pango_font_description_free(font); g_object_unref(layout);
}

static void paint_icon(cairo_t *cr, DesktopIcon *icon)
{
 double sx, sy;
 cairo_surface_get_device_scale(cairo_get_target(cr), &sx, &sy);
 if (!icon->rendered || icon->rendered_selected != icon->selected || icon->rendered_scale_x != sx || icon->rendered_scale_y != sy) {
  if (icon->rendered) cairo_surface_destroy(icon->rendered);
  icon->rendered = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ceil(84*sx), ceil(80*sy));
  cairo_surface_set_device_scale(icon->rendered, sx, sy);
  cairo_t *tile = cairo_create(icon->rendered);
  DesktopIcon local = *icon; local.x = local.y = 2;
  draw_icon(tile, &local); draw_label(tile, &local); cairo_destroy(tile);
  icon->rendered_selected = icon->selected; icon->rendered_scale_x = sx; icon->rendered_scale_y = sy;
 }
 cairo_set_source_surface(cr, icon->rendered, icon->x-2, icon->y-2); cairo_paint(cr);
}

static void paint_desktop(cairo_t *cr, int width, int height)
{
	draw_wallpaper(cr, width, height);
 double left, top, right, bottom;
 cairo_clip_extents(cr, &left, &top, &right, &bottom);
 for (guint i = 0; i < icon_count; i++) {
  DesktopIcon *icon = &icons[i];
  if (icon->x+82 <= left || icon->y+78 <= top || icon->x-2 >= right || icon->y-2 >= bottom) continue;
  paint_icon(cr, icon);
 }
 if (rubber_select) {
  double x = MIN(press_x, rubber_x), y = MIN(press_y, rubber_y);
  cairo_set_source_rgba(cr, .19, .42, .77, .18); cairo_rectangle(cr, x, y, fabs(rubber_x-press_x), fabs(rubber_y-press_y)); cairo_fill_preserve(cr);
  cairo_set_source_rgb(cr, .19, .42, .77); cairo_set_line_width(cr, 1); cairo_stroke(cr);
 }
}
static gboolean draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
 paint_desktop(cr, gtk_widget_get_allocated_width(widget), gtk_widget_get_allocated_height(widget));
 return FALSE;
}

static DesktopIcon *hit(double x, double y)
{
	for (int i = (int)icon_count-1; i >= 0; i--)
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

static void refresh(GtkMenuItem *item, gpointer data) { reload_icons(); gtk_widget_queue_draw(canvas); }
static void open_home(GtkMenuItem *item, gpointer data) { launch(&icons[0], gtk_get_current_event_time()); }

/* Preview uses the same wallpaper renderer as the desktop.  Its temporary
 * mode never changes the live desktop until Apply or OK is selected. */
static GtkWidget *properties_window;

static gboolean preview_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	GtkWidget *combo = g_object_get_data(G_OBJECT(properties_window), "wallpaper-mode");
	int saved_mode = wall_mode, mode = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
	int w = gtk_widget_get_allocated_width(widget), x = (w - 180) / 2;
	cairo_set_source_rgb(cr, .72, .72, .69);
	cairo_rectangle(cr, x, 3, 180, 136); cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, .35, .35, .34); cairo_set_line_width(cr, 1); cairo_stroke(cr);
	cairo_set_source_rgb(cr, .93, .93, .9); cairo_rectangle(cr, x+2, 5, 176, 132); cairo_fill(cr);
	cairo_set_source_rgb(cr, .12, .12, .12); cairo_rectangle(cr, x+9, 12, 162, 110); cairo_fill(cr);
	cairo_save(cr); cairo_rectangle(cr, x+12, 15, 156, 104); cairo_clip(cr);
	cairo_translate(cr, x+12, 15);
	/* Render at desktop resolution, then scale the entire virtual monitor. */
	cairo_scale(cr, 156.0 / MAX(1, gtk_widget_get_allocated_width(canvas)),
		104.0 / MAX(1, gtk_widget_get_allocated_height(canvas)));
	wall_mode = mode;
	draw_wallpaper(cr, gtk_widget_get_allocated_width(canvas), gtk_widget_get_allocated_height(canvas));
	wall_mode = saved_mode; cairo_restore(cr);
	cairo_set_source_rgb(cr, .65, .65, .62); cairo_rectangle(cr, x+74, 139, 32, 11); cairo_fill(cr);
	cairo_rectangle(cr, x+55, 150, 70, 5); cairo_fill(cr);
	cairo_set_source_rgb(cr, .2, .7, .1); cairo_rectangle(cr, x+160, 129, 3, 2); cairo_fill(cr);
	return FALSE;
}

static void properties_changed(GtkComboBox *combo, gpointer data)
{
	gtk_widget_queue_draw(GTK_WIDGET(data));
	gtk_dialog_set_response_sensitive(GTK_DIALOG(properties_window), GTK_RESPONSE_APPLY, TRUE);
}

static void apply_wallpaper(gint response_id)
{
 if (!properties_window) return;
 GtkWidget *combo = g_object_get_data(G_OBJECT(properties_window), "wallpaper-mode");
 int mode = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
 if (mode >= WALL_STRETCH && mode <= WALL_TILE) wall_mode = mode;
 gtk_widget_queue_draw(canvas);
 gtk_dialog_set_response_sensitive(GTK_DIALOG(properties_window), GTK_RESPONSE_APPLY, FALSE);
 if (response_id == GTK_RESPONSE_OK) gtk_widget_destroy(properties_window);
}

static void display_applied(gboolean kept, gpointer data)
{
 if (kept) apply_wallpaper(GPOINTER_TO_INT(data));
}

static void display_changed(gpointer data)
{
 if (properties_window) {
  GtkWidget *settings = g_object_get_data(G_OBJECT(properties_window), "display-settings-page");
  GtkWidget *combo = g_object_get_data(G_OBJECT(properties_window), "wallpaper-mode");
  gboolean pending = settings && xp_display_settings_dirty(settings);
  if (combo) pending |= gtk_combo_box_get_active(GTK_COMBO_BOX(combo)) != wall_mode;
  gtk_dialog_set_response_sensitive(GTK_DIALOG(properties_window), GTK_RESPONSE_APPLY, pending);
 }
}

static void properties_response(GtkDialog *dialog, gint response_id, gpointer data)
{
 if (response_id == GTK_RESPONSE_OK || response_id == GTK_RESPONSE_APPLY) {
  GtkWidget *settings = g_object_get_data(G_OBJECT(dialog), "display-settings-page");
  if (xp_display_settings_busy(settings)) return;
  if (xp_display_settings_dirty(settings)) {
   xp_display_settings_apply(settings, display_applied, GINT_TO_POINTER(response_id));
   return;
  }
  apply_wallpaper(response_id);
 } else gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void properties(GtkMenuItem *item, gpointer data)
{
	GtkWidget *box, *notebook, *page, *preview, *row, *label, *combo, *actions;
	if (properties_window) { gtk_window_present(GTK_WINDOW(properties_window)); return; }
	properties_window = gtk_dialog_new_with_buttons("Display Properties", GTK_WINDOW(window),
		GTK_DIALOG_DESTROY_WITH_PARENT, "OK", GTK_RESPONSE_OK, "Cancel", GTK_RESPONSE_CANCEL,
		"_Apply", GTK_RESPONSE_APPLY, NULL);
	g_signal_connect(properties_window, "destroy", G_CALLBACK(gtk_widget_destroyed), &properties_window);
	gtk_widget_set_name(properties_window, "xpwm-properties");
	gtk_window_set_resizable(GTK_WINDOW(properties_window), FALSE);
	gtk_window_set_default_size(GTK_WINDOW(properties_window), 400, -1);
	box = gtk_dialog_get_content_area(GTK_DIALOG(properties_window));
	gtk_container_set_border_width(GTK_CONTAINER(box), 7);
	notebook = gtk_notebook_new(); page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page, gtk_label_new("Desktop"));
	gtk_box_pack_start(GTK_BOX(box), notebook, TRUE, TRUE, 0);
	preview = gtk_drawing_area_new(); gtk_widget_set_size_request(preview, 240, 160);
	g_signal_connect(preview, "draw", G_CALLBACK(preview_draw), NULL);
	gtk_box_pack_start(GTK_BOX(page), preview, FALSE, FALSE, 0);
	label = gtk_label_new("Background:"); gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(page), label, FALSE, FALSE, 0);
	row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	label = gtk_label_new(wallpaper || wallpaper_surface ? "Bliss" : "Windows XP");
	gtk_widget_set_hexpand(label, TRUE); gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_box_pack_start(GTK_BOX(row), label, TRUE, TRUE, 0);
	label = gtk_label_new_with_mnemonic("_Position:");
	gtk_box_pack_start(GTK_BOX(row), label, FALSE, FALSE, 0);
	combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Stretch");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Center");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Tile");
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), wall_mode);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), combo);
	gtk_box_pack_start(GTK_BOX(row), combo, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(page), row, FALSE, FALSE, 0);
	g_object_set_data(G_OBJECT(properties_window), "wallpaper-mode", combo);
 GtkWidget *settings = xp_display_settings_new(GTK_WINDOW(properties_window), display_changed, NULL);
 gtk_notebook_append_page(GTK_NOTEBOOK(notebook), settings, gtk_label_new("Settings"));
 g_object_set_data(G_OBJECT(properties_window), "display-settings-page", settings);
 g_object_set_data(G_OBJECT(properties_window), "properties-notebook", notebook);
	g_signal_connect(combo, "changed", G_CALLBACK(properties_changed), preview);
	g_signal_connect(properties_window, "response", G_CALLBACK(properties_response), NULL);
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	actions = gtk_dialog_get_action_area(GTK_DIALOG(properties_window));
	G_GNUC_END_IGNORE_DEPRECATIONS
	gtk_container_set_border_width(GTK_CONTAINER(actions), 7); gtk_box_set_spacing(GTK_BOX(actions), 6);
	xp_style_apply(properties_window);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(properties_window), GTK_RESPONSE_APPLY, FALSE);
	gtk_widget_show_all(properties_window);
}

#include "desktop-surface.h"

static GtkWidget *desktop_menu_new(void)
{
	GtkWidget *menu = gtk_menu_new(), *item;
 GPtrArray *files = selected_files(); gboolean selected = FALSE;
 for (guint i = 0; i < icon_count; i++) if (icons[i].selected) selected = TRUE;
 if (selected) {
  action(menu, "Open", G_CALLBACK(open_selected), NULL, TRUE);
  if (files->len) {
   action(menu, "Cut", G_CALLBACK(copy_selected), GINT_TO_POINTER(1), !operation_cancel);
   action(menu, "Copy", G_CALLBACK(copy_selected), NULL, TRUE);
   action(menu, "Rename", G_CALLBACK(rename_selected), NULL, files->len == 1 && !operation_cancel);
   action(menu, "Delete", G_CALLBACK(trash_selected), NULL, !operation_cancel);
  }
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
 }
 item = gtk_menu_item_new_with_label("Arrange Icons By"); gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), arrange_menu()); gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
 action(menu, "Refresh", G_CALLBACK(refresh), NULL, TRUE);
 action(menu, "Paste", G_CALLBACK(paste_files), NULL, !operation_cancel);
 action(menu, "New Folder", G_CALLBACK(new_folder), NULL, !operation_cancel);
 action(menu, "Open Home folder", G_CALLBACK(open_home), NULL, TRUE);
 gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
 action(menu, "Properties", G_CALLBACK(properties), NULL, TRUE);
 g_ptr_array_unref(files);
 g_signal_connect(menu, "selection-done", G_CALLBACK(destroy_menu), NULL);
	xp_style_apply(menu);
	gtk_widget_show_all(menu);
	return menu;
}

static gboolean button(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
 DesktopIcon *icon = hit(event->x, event->y);
 gtk_widget_grab_focus(canvas);
 if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
  if (!icon || !icon->selected) { clear_selection(); if (icon) icon->selected = TRUE; }
  if (icon) focused_icon = icon-icons;
  gtk_widget_queue_draw(canvas);
  GtkWidget *menu = desktop_menu_new(); gtk_menu_attach_to_widget(GTK_MENU(menu), canvas, NULL); gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event); return TRUE;
 }
 if (event->button != 1) return FALSE;
 if (event->type == GDK_BUTTON_PRESS) {
  gboolean control = event->state & GDK_CONTROL_MASK, shift = event->state & GDK_SHIFT_MASK;
  plain_click = icon && !control && !shift;
  pointer_down = TRUE; press_x = event->x; press_y = event->y; rubber_x = press_x; rubber_y = press_y;
  g_clear_pointer(&drag_start, g_free); g_clear_pointer(&selection_base, g_free);
  if (icon) {
   int index = icon-icons;
   if (shift) { if (!control) clear_selection(); for (int i = MIN(selection_anchor, index); i <= MAX(selection_anchor, index); i++) icons[i].selected = TRUE; }
   else if (control) icon->selected = !icon->selected;
   else if (!icon->selected) { clear_selection(); icon->selected = TRUE; }
   if (!shift) selection_anchor = index;
   focused_icon = index; drag_start = g_new(int, icon_count*2);
   for (guint i = 0; i < icon_count; i++) { drag_start[i*2] = icons[i].x; drag_start[i*2+1] = icons[i].y; }
  } else {
   if (!control && !shift) clear_selection();
   rubber_toggle = control; rubber_add = shift; rubber_select = TRUE;
   selection_base = g_new(gboolean, icon_count); for (guint i = 0; i < icon_count; i++) selection_base[i] = icons[i].selected;
  }
  gtk_widget_queue_draw(canvas);
 } else if (event->type == GDK_2BUTTON_PRESS && icon) { pointer_down = rubber_select = dragging = FALSE; launch(icon, event->time); }
 return TRUE;
}

static gboolean test_ready(gpointer data)
{
	static int stage;
	const char *marker = g_getenv("XPWM_DESKTOP_TEST");
	const char *screenshot = g_getenv("XPWM_DESKTOP_SCREENSHOT");
	if (!marker) return G_SOURCE_REMOVE;
	if (stage == 0) {
		properties(NULL, NULL);
		g_assert(properties_window);
		GtkWidget *first = properties_window;
		properties(NULL, NULL); g_assert(properties_window == first);
		stage++;
		return G_SOURCE_CONTINUE;
	}
 if (stage == 10) {
  char *quoted = g_shell_quote(g_getenv("XPWM_DISPLAY_SETTINGS_SCREENSHOT"));
  char *command = g_strdup_printf("grim %s", quoted);
  g_assert(g_spawn_command_line_sync(command, NULL, NULL, NULL, NULL));
  g_free(command); g_free(quoted);
  g_unsetenv("XPWM_DISPLAY_SETTINGS_SCREENSHOT");
  stage = 1;
 }
	if (stage == 1) {
		if (g_getenv("XPWM_DISPLAY_SETTINGS_SCREENSHOT")) {
   GtkWidget *notebook = g_object_get_data(G_OBJECT(properties_window), "properties-notebook");
   gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 1);
   stage = 10;
   return G_SOURCE_CONTINUE;
  }
  if (g_getenv("XPWM_PROPERTIES_SCREENSHOT")) {
			char *quoted = g_shell_quote(g_getenv("XPWM_PROPERTIES_SCREENSHOT"));
			char *command = g_strdup_printf("grim %s", quoted);
			g_assert(g_spawn_command_line_sync(command, NULL, NULL, NULL, NULL));
			g_free(command); g_free(quoted);
		}
		{
			int saved = wall_mode, next = (saved + 1) % 3;
			GtkWidget *combo = g_object_get_data(G_OBJECT(properties_window), "wallpaper-mode");
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), next);
			g_assert_cmpint(wall_mode, ==, saved);
			gtk_dialog_response(GTK_DIALOG(properties_window), GTK_RESPONSE_APPLY);
			g_assert_cmpint(wall_mode, ==, next);
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), saved);
			gtk_dialog_response(GTK_DIALOG(properties_window), GTK_RESPONSE_CANCEL);
			g_assert(!properties_window); g_assert_cmpint(wall_mode, ==, next);
			wall_mode = saved;
		}
		stage++;
		return G_SOURCE_CONTINUE;
	}
	if (loading) return G_SOURCE_CONTINUE;
 g_assert(icon_count >= 5);
	g_assert(wallpaper != NULL || wallpaper_surface != NULL || g_getenv("XPWM_ALLOW_NO_WALLPAPER"));
	{
		GtkWidget *menu = desktop_menu_new();
		GList *items = gtk_container_get_children(GTK_CONTAINER(menu));
		g_assert_cmpuint(g_list_length(items), >=, 7);
		g_assert_cmpstr(gtk_menu_item_get_label(GTK_MENU_ITEM(g_list_last(items)->data)), ==, "Properties");
		g_list_free(items); gtk_widget_destroy(menu);
	}
	if (screenshot) {
		char *quoted = g_shell_quote(screenshot);
		char *command = g_strdup_printf("grim %s", quoted);
		g_assert(g_spawn_command_line_sync(command, NULL, NULL, NULL, NULL));
		g_free(command); g_free(quoted);
	}
	g_assert(g_file_set_contents(marker, "PASS: desktop wallpaper, XP icons, layer surface, Display Properties preview/apply/cancel\n", -1, NULL));
	gtk_main_quit(); return G_SOURCE_REMOVE;
}


#include "desktop-input-test.h"

int main(int argc, char **argv)
{
	luna_init();
	gtk_init(&argc, &argv); load_wallpaper(); init_icons();
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_layer_init_for_window(GTK_WINDOW(window));
	gtk_layer_set_namespace(GTK_WINDOW(window), "xpwm-desktop"); gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_BACKGROUND);
	gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
	gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
	gtk_layer_set_exclusive_zone(GTK_WINDOW(window), 0);
	canvas = gtk_drawing_area_new(); gtk_widget_add_events(canvas, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
 gtk_widget_set_can_focus(canvas, TRUE);
	gtk_container_add(GTK_CONTAINER(window), canvas); g_signal_connect(canvas, "draw", G_CALLBACK(draw), NULL);
	g_signal_connect(canvas, "button-press-event", G_CALLBACK(button), NULL);
	g_signal_connect(canvas, "motion-notify-event", G_CALLBACK(desktop_motion), NULL);
 g_signal_connect(canvas, "button-release-event", G_CALLBACK(desktop_release), NULL);
 g_signal_connect(canvas, "size-allocate", G_CALLBACK(canvas_resized), NULL);
 g_signal_connect(window, "key-press-event", G_CALLBACK(desktop_key), NULL);
 GtkTargetEntry targets[] = {{"text/uri-list", 0, 0}};
 gtk_drag_dest_set(canvas, GTK_DEST_DEFAULT_ALL, targets, 1, GDK_ACTION_COPY);
 g_signal_connect(canvas, "drag-data-received", G_CALLBACK(desktop_drop), NULL);
 g_signal_connect(window, "destroy", G_CALLBACK(desktop_destroyed), NULL);
	gtk_widget_show_all(window); reload_icons();
	if (g_getenv("XPWM_DESKTOP_FILES_TEST")) {
  g_signal_connect(canvas, "draw", G_CALLBACK(desktop_test_damage), NULL);
  g_timeout_add(100, desktop_input_test, NULL);
 }
	if (g_getenv("XPWM_DESKTOP_TEST")) g_timeout_add(350, test_ready, NULL);
	gtk_main();
	g_clear_object(&wallpaper);
 if (wallpaper_cache) cairo_surface_destroy(wallpaper_cache);
	if (wallpaper_surface) cairo_surface_destroy(wallpaper_surface);
	g_free(wallpaper_path);
 desktop_dead = TRUE; if (list_cancel) g_cancellable_cancel(list_cancel); if (operation_cancel) g_cancellable_cancel(operation_cancel);
 if (reload_timer) g_source_remove(reload_timer);
 g_clear_object(&desktop_monitor);
 for (guint i = 0; i < icon_count; i++) free_icon(&icons[i]);
 g_free(icons);
 g_clear_object(&desktop_folder); g_key_file_unref(desktop_layout); if (select_after) g_hash_table_unref(select_after);
 g_free(drag_start); g_free(selection_base); return 0;
}
