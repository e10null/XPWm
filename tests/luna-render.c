/* Synthetic two-state strip: test shared rendering without Microsoft assets. */
#include <assert.h>
#include <stdint.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "../shared/luna.h"

static uint32_t pixel(cairo_surface_t *surface, int x, int y)
{
	cairo_surface_flush(surface);
	return *(uint32_t *)(cairo_image_surface_get_data(surface) +
		y * cairo_image_surface_get_stride(surface) + x * 4);
}

int main(void)
{
	char *directory = g_dir_make_tmp("xpwm-luna-render-XXXXXX", NULL);
	char *path, *manifest;
	cairo_surface_t *strip = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 3, 6);
	cairo_surface_t *target = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 12, 12);
	cairo_t *source = cairo_create(strip), *cr = cairo_create(target);
	assert(directory);
	cairo_set_source_rgb(source, 1, 0, 0); cairo_paint(source);
	cairo_set_source_rgb(source, 0, 1, 0); cairo_rectangle(source, 0, 3, 3, 3); cairo_fill(source);
	cairo_set_source_rgb(source, 0, 0, 1); cairo_rectangle(source, 0, 0, 1, 1); cairo_fill(source);
	path = g_build_filename(directory, "strip.png", NULL);
	assert(cairo_surface_write_to_png(strip, path) == CAIRO_STATUS_SUCCESS);
	manifest = g_build_filename(directory, "theme.ini", NULL);
	assert(g_file_set_contents(manifest,
		"[stretch]\nfile=strip.png\ncount=2\nmargins=1;1;1;1;\ntile=false\n"
		"[tile]\nfile=strip.png\ncount=2\nmargins=1;1;1;1;\ntile=true\n"
		"[bad]\nfile=strip.png\ncount=4\nmargins=0;0;0;0;\n", -1, NULL));
	g_setenv("XPWM_THEME_DIR", directory, TRUE);
	assert(luna_draw(cr, "stretch", 0, 0, 0, 12, 12));
	assert(pixel(target, 0, 0) == 0xff0000ff);
	assert(pixel(target, 1, 1) == 0xffff0000);
	assert(pixel(target, 11, 11) == 0xffff0000);
	assert(luna_draw(cr, "tile", 1, 0, 0, 12, 12));
	assert(pixel(target, 0, 0) == 0xff00ff00);
	assert(pixel(target, 6, 6) == 0xff00ff00);
	assert(!luna_draw(cr, "stretch", 2, 0, 0, 12, 12));
	assert(!luna_draw(cr, "missing", 0, 0, 0, 12, 12));
	assert(!luna_draw(cr, "bad", 0, 0, 0, 12, 12));
	assert(luna_draw(cr, "tile", 0, 0, 0, 1, 1));
	assert(cairo_status(cr) == CAIRO_STATUS_SUCCESS);
	luna_finish();
	cairo_destroy(cr); cairo_destroy(source);
	cairo_surface_destroy(target); cairo_surface_destroy(strip);
	g_unlink(path); g_unlink(manifest); g_rmdir(directory);
	g_free(path); g_free(manifest); g_free(directory);
	g_print("PASS: Luna state strips, nine-slice corners, tiling, invalid roles, small targets\n");
	return 0;
}
