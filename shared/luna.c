/* Small shared Cairo renderer for local, provenance-indexed Luna PNG strips. */
#include <glib.h>
#include <fontconfig/fontconfig.h>
#include "luna.h"

typedef struct {
	cairo_surface_t *surface;
	int count, width, height;
	gint *margins;
	gboolean tile;
} Image;
static GHashTable *images;
static int attempted;
static GHashTable *icons;

static int xp_image(cairo_t *cr, const char *relative, double x, double y)
{
	char *key, *exe, *dir, *root, *path;
	cairo_surface_t *surface;
	const char *override = g_getenv("XPWM_ASSET_ROOT");
	if (!icons) icons = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)cairo_surface_destroy);
	key = g_strdup(relative);
	surface = g_hash_table_lookup(icons, key);
	if (!surface) {
		exe = g_file_read_link("/proc/self/exe", NULL);
		dir = exe ? g_path_get_dirname(exe) : g_get_current_dir();
		root = override ? g_strdup(override) : g_canonicalize_filename("..", dir);
		path = g_build_filename(root, "assets", "local", relative, NULL);
		surface = cairo_image_surface_create_from_png(path);
		g_free(path); g_free(root); g_free(dir); g_free(exe);
		g_hash_table_insert(icons, key, surface);
	} else g_free(key);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) return 0;
	cairo_save(cr); cairo_new_path(cr); cairo_set_source_surface(cr, surface, x, y); cairo_paint(cr); cairo_restore(cr);
	return 1;
}

int xp_icon(cairo_t *cr, int resource, int size, double x, double y)
{
	char *path = g_strdup_printf("xp-icons/shell32.dll/GROUP_ICON/%d/1033-%d.png", resource, size);
	int result = xp_image(cr, path, x, y);
	g_free(path);
	return result;
}

int xp_start_flag(cairo_t *cr, double x, double y)
{
	return xp_image(cr, "xp-bitmaps/explorer.exe-BITMAP-143-1033.png", x, y);
}

int xp_bitmap(cairo_t *cr, const char *relative, double x, double y)
{
	if (!relative || !*relative) return 0;
	return xp_image(cr, relative, x, y);
}

int xp_bitmap_region(cairo_t *cr, const char *relative, double sx, double sy,
        double width, double height, double x, double y)
{
	int result;
	if (!relative || !*relative || sx < 0 || sy < 0 || width <= 0 || height <= 0) return 0;
	cairo_save(cr); cairo_new_path(cr);
	cairo_rectangle(cr, x, y, width, height); cairo_clip(cr);
	result = xp_image(cr, relative, x - sx, y - sy);
	cairo_restore(cr);
	return result;
}

static gboolean has_theme_file(const char *directory)
{
	char *path = g_build_filename(directory, "theme.ini", NULL);
	gboolean found = g_file_test(path, G_FILE_TEST_IS_REGULAR);
	g_free(path);
	return found;
}

/* Resolve the development tree without making the current working directory
 * part of the runtime contract.  XPWM_THEME_DIR is an explicit override; the
 * other two locations make `make nested` and test launchers work from a VT,
 * a build directory, or a session manager. */
static char *theme_directory(void)
{
	const char *override = g_getenv("XPWM_THEME_DIR");
	const char *root = g_getenv("XPWM_ASSET_ROOT");
	char *candidate, *exe, *exe_dir, *project;

	if (override && *override)
		return g_strdup(override);
	if (g_file_test("assets/local/luna-blue-complete/theme.ini", G_FILE_TEST_IS_REGULAR))
		return g_strdup("assets/local/luna-blue-complete");
	if (g_file_test("assets/local/luna-blue-start/theme.ini", G_FILE_TEST_IS_REGULAR))
		return g_strdup("assets/local/luna-blue-start");
	if (g_file_test("assets/local/luna-blue/theme.ini", G_FILE_TEST_IS_REGULAR))
		return g_strdup("assets/local/luna-blue");
	if (root && *root) {
		candidate = g_build_filename(root, "assets", "local", "luna-blue-complete", NULL);
		if (has_theme_file(candidate)) return candidate;
		g_free(candidate);
		candidate = g_build_filename(root, "assets", "local", "luna-blue-start", NULL);
		if (has_theme_file(candidate))
			return candidate;
		g_free(candidate);
		candidate = g_build_filename(root, "assets", "local", "luna-blue", NULL);
		if (has_theme_file(candidate))
			return candidate;
		g_free(candidate);
	}
	/* Installed layouts are allowed to put the executable in shell/ below the
	 * project root.  /proc/self/exe is Linux-specific, matching this Linux WM. */
	exe = g_file_read_link("/proc/self/exe", NULL);
	if (exe) {
		exe_dir = g_path_get_dirname(exe);
		project = g_canonicalize_filename("..", exe_dir);
		g_free(exe_dir); g_free(exe);
		candidate = g_build_filename(project, "assets", "local", "luna-blue-complete", NULL);
		if (has_theme_file(candidate)) { g_free(project); return candidate; }
		g_free(candidate);
		candidate = g_build_filename(project, "assets", "local", "luna-blue-start", NULL);
		if (has_theme_file(candidate)) {
			g_free(project); return candidate;
		}
		g_free(candidate);
		candidate = g_build_filename(project, "assets", "local", "luna-blue", NULL);
		if (has_theme_file(candidate)) {
			g_free(project); return candidate;
		}
		g_free(candidate); g_free(project);
	}
	return g_strdup("assets/local/luna-blue");
}

static void destroy_image(gpointer data)
{
	Image *image = data;
	cairo_surface_destroy(image->surface);
	g_free(image->margins); g_free(image);
}

static void report_font(const char *name)
{
	FcPattern *query, *match;
	FcResult result;
	FcChar8 *family = NULL, *file = NULL;
	query = FcNameParse((const FcChar8 *)name);
	FcConfigSubstitute(NULL, query, FcMatchPattern);
	FcDefaultSubstitute(query);
	match = FcFontMatch(NULL, query, &result);
	if (match) {
		FcPatternGetString(match, FC_FAMILY, 0, &family);
		FcPatternGetString(match, FC_FILE, 0, &file);
	}
	g_printerr("XPWm: XP font %-20s -> %s (%s)\n", name,
		family ? (const char *)family : "unavailable", file ? (const char *)file : "no file");
	if (match) FcPatternDestroy(match);
	FcPatternDestroy(query);
}

int luna_init(void)
{
	GKeyFile *ini;
	char *directory;
	char *path, **groups;
	gsize count, i;
	GError *error = NULL;
	GDir *fonts;
	const char *font;
	if (attempted) return images != NULL;
	attempted = 1;
	directory = theme_directory();
	path = g_build_filename(directory, "theme.ini", NULL);
	ini = g_key_file_new();
	if (!g_key_file_load_from_file(ini, path, G_KEY_FILE_NONE, &error)) {
		g_printerr("XPWm: local Luna assets unavailable (%s); using fallback drawing\n", error->message);
		g_clear_error(&error); g_key_file_unref(ini); g_free(path); g_free(directory); return 0;
	}
	g_free(path);
	images = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, destroy_image);
	groups = g_key_file_get_groups(ini, &count);
	for (i = 0; i < count; i++) {
		Image *image = g_new0(Image, 1);
		gsize n = 0;
		char *file = g_key_file_get_string(ini, groups[i], "file", NULL);
		if (!file) { g_free(image); continue; }
		path = g_build_filename(directory, file, NULL); g_free(file);
		image->surface = cairo_image_surface_create_from_png(path); g_free(path);
		image->count = g_key_file_get_integer(ini, groups[i], "count", NULL);
		image->margins = g_key_file_get_integer_list(ini, groups[i], "margins", &n, NULL);
		image->tile = g_key_file_get_boolean(ini, groups[i], "tile", NULL);
		if (cairo_surface_status(image->surface) != CAIRO_STATUS_SUCCESS || image->count < 1 || n != 4 || !image->margins) {
			g_printerr("XPWm: invalid Luna role %s\n", groups[i]); destroy_image(image); continue;
		}
		image->width = cairo_image_surface_get_width(image->surface);
		image->height = cairo_image_surface_get_height(image->surface) / image->count;
		if (cairo_image_surface_get_height(image->surface) % image->count ||
				image->height < 1 || image->margins[0] < 0 || image->margins[1] < 0 ||
				image->margins[2] < 0 || image->margins[3] < 0 ||
				image->margins[0] + image->margins[1] > image->width ||
				image->margins[2] + image->margins[3] > image->height) {
			destroy_image(image); continue;
		}
		g_hash_table_insert(images, g_strdup(groups[i]), image);
	}
	g_strfreev(groups); g_key_file_unref(ini);
	path = g_build_filename(directory, "fonts", NULL);
	fonts = g_dir_open(path, 0, NULL);
	if (fonts) {
		while ((font = g_dir_read_name(fonts))) {
			char *filename = g_build_filename(path, font, NULL);
			if (!FcConfigAppFontAddFile(NULL, (const FcChar8 *)filename))
				g_printerr("XPWm: unable to load local font %s\n", filename);
			g_free(filename);
		}
		g_dir_close(fonts);
	}
	/* Make the application faces visible to Pango immediately and leave a
	 * diagnostic trail so a session log proves which font was selected. */
	FcConfigBuildFonts(NULL);
	report_font("Tahoma");
	report_font("Arial");
	report_font("Courier New");
	report_font("Times New Roman");
	report_font("Tahoma:style=Bold");
	report_font("Trebuchet MS");
	report_font("Trebuchet MS:style=Bold");
	report_font("Franklin Gothic Medium");
	report_font("Franklin Gothic Medium:style=Italic");
	g_free(path);
	g_printerr("XPWm: loaded %u original Luna image roles from %s\n", g_hash_table_size(images), directory);
	g_free(directory);
	return 1;
}

static void patch(cairo_t *cr, Image *image, int state, double sx, double sy,
		double sw, double sh, double x, double y, double w, double h)
{
	cairo_pattern_t *pattern;
	if (sw <= 0 || sh <= 0 || w <= 0 || h <= 0) return;
	cairo_save(cr);
	/* Adjacent slices share an edge. Antialiased fractional clip masks would
	 * each cover only part of the seam and expose the desktop underneath. */
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	cairo_rectangle(cr, x, y, w, h); cairo_clip(cr);
	cairo_translate(cr, x, y); cairo_scale(cr, w / sw, h / sh);
	cairo_set_source_surface(cr, image->surface, -sx, -sy - state * image->height);
	pattern = cairo_get_source(cr);
	cairo_pattern_set_filter(pattern, CAIRO_FILTER_NEAREST);
	cairo_paint(cr); cairo_restore(cr);
}

int luna_draw(cairo_t *cr, const char *role, int state, double x, double y, double w, double h)
{
	Image *image;
	double sx[4], sy[4], dx[4], dy[4], left, right, top, bottom;
	int a, b;
	if (!luna_init() || !(image = g_hash_table_lookup(images, role))) return 0;
	if (state < 0 || state >= image->count || w <= 0 || h <= 0) return 0;
	left = image->margins[0]; right = image->margins[1];
	top = image->margins[2]; bottom = image->margins[3];
	sx[0] = 0; sx[1] = left; sx[2] = image->width - right; sx[3] = image->width;
	sy[0] = 0; sy[1] = top; sy[2] = image->height - bottom; sy[3] = image->height;
	if (left + right > w) { double scale = w / (left + right); left *= scale; right *= scale; }
	if (top + bottom > h) { double scale = h / (top + bottom); top *= scale; bottom *= scale; }
	dx[0] = x; dx[1] = x + left; dx[2] = x + w - right; dx[3] = x + w;
	dy[0] = y; dy[1] = y + top; dy[2] = y + h - bottom; dy[3] = y + h;
	for (b = 0; b < 3; b++) for (a = 0; a < 3; a++) {
		double sw = sx[a+1]-sx[a], sh = sy[b+1]-sy[b];
		double dw = dx[a+1]-dx[a], dh = dy[b+1]-dy[b];
		if (image->tile && sw > 0 && sh > 0 && (a == 1 || b == 1)) {
			double px, py, pw, ph;
			for (py = 0; py < dh; py += b == 1 ? sh : dh)
				for (px = 0; px < dw; px += a == 1 ? sw : dw) {
					pw = a == 1 ? MIN(sw, dw-px) : dw;
					ph = b == 1 ? MIN(sh, dh-py) : dh;
					patch(cr, image, state, sx[a], sy[b], a == 1 ? pw : sw,
						b == 1 ? ph : sh, dx[a]+px, dy[b]+py, pw, ph);
				}
		} else patch(cr, image, state, sx[a], sy[b], sw, sh, dx[a], dy[b], dw, dh);
	}
	return 1;
}

void luna_finish(void)
{
	if (icons) g_hash_table_destroy(icons);
	icons = NULL;
	if (images) g_hash_table_destroy(images);
	images = NULL; attempted = 0;
}
