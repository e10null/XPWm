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
	/* Opaque source strips must stay opaque when opposing margins shrink to
	 * fractional destination pixels, as on the three-pixel window frame. */
	for (int size = 1; size <= 11; size++) {
		cairo_save(cr); cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR); cairo_paint(cr); cairo_restore(cr);
		assert(luna_draw(cr, "stretch", 0, 0, 0, size, size));
		for (int y = 0; y < size; y++) for (int x = 0; x < size; x++)
			assert((pixel(target, x, y) >> 24) == 255);
	}
	assert(cairo_status(cr) == CAIRO_STATUS_SUCCESS);
	/* Cropping a native strip must select the requested frame and never
	 * paint neighboring states or pixels outside its destination. */
	{
		char *local = g_build_filename(directory, "assets", "local", NULL);
		char *bitmap = g_build_filename(local, "test-strip.png", NULL);
		assert(g_mkdir_with_parents(local, 0700) == 0);
		assert(cairo_surface_write_to_png(strip, bitmap) == CAIRO_STATUS_SUCCESS);
		g_setenv("XPWM_ASSET_ROOT", directory, TRUE);
		cairo_set_source_rgb(cr, 0, 0, 0); cairo_paint(cr);
		assert(xp_bitmap_region(cr, "test-strip.png", 0, 3, 3, 3, 4, 4));
		assert(pixel(target, 4, 4) == 0xff00ff00);
		assert(pixel(target, 6, 6) == 0xff00ff00);
		assert(pixel(target, 3, 4) == 0xff000000);
		assert(pixel(target, 7, 4) == 0xff000000);
		assert(!xp_bitmap_region(cr, "test-strip.png", -1, 0, 3, 3, 0, 0));
		g_unlink(bitmap); g_rmdir(local);
		g_free(bitmap); g_free(local);
		local = g_build_filename(directory, "assets", NULL); g_rmdir(local); g_free(local);
	}
	luna_finish();
	cairo_destroy(cr); cairo_destroy(source);
	cairo_surface_destroy(target); cairo_surface_destroy(strip);
	g_unlink(path); g_unlink(manifest); g_rmdir(directory);
	g_free(path); g_free(manifest); g_free(directory);
	g_print("PASS: Luna state strips, nine-slice corners, tiling, invalid roles, small targets, native bitmap-region clipping\n");
	return 0;
}
