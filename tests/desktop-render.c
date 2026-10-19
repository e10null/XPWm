#define main desktop_original_main
#include "../shell/desktop.c"
#undef main
#include <stdio.h>
static void fixture(void)
{
 icon_count = 32; icons = g_new0(DesktopIcon, icon_count);
 for (guint i = 0; i < icon_count; i++) {
  icons[i] = (DesktopIcon){.label=g_strdup_printf("Desktop item %u", i), .kind="folder", .x=20+(i/10)*96, .y=20+(i%10)*80, .selected=TRUE};
 }
}
static void render(cairo_surface_t *surface, gboolean partial, int x, int y)
{
 cairo_t *cr = cairo_create(surface);
 if (partial) { cairo_rectangle(cr, x-2, y-2, 180, 82); cairo_clip(cr); }
 paint_desktop(cr, 1920, 1080); cairo_destroy(cr); cairo_surface_flush(surface);
}
static void assert_same(cairo_surface_t *a, cairo_surface_t *b)
{
 cairo_surface_flush(a); cairo_surface_flush(b);
 g_assert_cmpint(cairo_surface_status(a), ==, CAIRO_STATUS_SUCCESS);
 g_assert_cmpint(cairo_surface_status(b), ==, CAIRO_STATUS_SUCCESS);
 gsize bytes = cairo_image_surface_get_stride(a)*cairo_image_surface_get_height(a);
 if (memcmp(cairo_image_surface_get_data(a), cairo_image_surface_get_data(b), bytes)) {
  cairo_surface_write_to_png(a, "/tmp/xpwm-damage-partial.png"); cairo_surface_write_to_png(b, "/tmp/xpwm-damage-full.png");
  unsigned int *pa = (void *)cairo_image_surface_get_data(a), *pb = (void *)cairo_image_surface_get_data(b);
  for (gsize i = 0; i < bytes/4; i++) if (pa[i] != pb[i]) { g_printerr("First mismatch %zu,%zu: %08x vs %08x\n", i%cairo_image_surface_get_width(a), i/cairo_image_surface_get_width(a), pa[i], pb[i]); break; }
  g_assert_not_reached();
 }
}
static void verify_damage(int scale)
{
 cairo_surface_t *partial = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1920*scale, 1080*scale);
 cairo_surface_t *full = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1920*scale, 1080*scale);
 cairo_surface_set_device_scale(partial, scale, scale); cairo_surface_set_device_scale(full, scale, scale);
 for (int mode = WALL_STRETCH; mode <= WALL_TILE; mode++) {
  wall_mode = mode;
  render(partial, FALSE, 0, 0); render(full, FALSE, 0, 0); assert_same(partial, full);
  for (int step = 0; step < 8; step++) {
   cairo_t *cr = cairo_create(partial);
   /* Follow a drag across another icon and back, including the label shadow. */
   cairo_rectangle(cr, icons[0].x-2, icons[0].y-2, 84, 80);
   icons[0].x += step%2 ? -17 : 43; icons[0].y += step%2 ? 13 : 27;
   cairo_rectangle(cr, icons[0].x-2, icons[0].y-2, 84, 80); cairo_clip(cr);
   paint_desktop(cr, 1920, 1080); cairo_destroy(cr);
   render(full, FALSE, 0, 0); assert_same(partial, full);
  }
  /* Band resizing, including icons whose selection tint changes outside its fill. */
  rubber_select = TRUE; press_x = 5; press_y = 5; rubber_x = 180; rubber_y = 180;
  render(partial, FALSE, 0, 0);
  cairo_t *cr = cairo_create(partial); cairo_rectangle(cr, 3, 3, 179, 179);
  rubber_x = 70; rubber_y = 35; cairo_rectangle(cr, 3, 3, 69, 34);
  cairo_rectangle(cr, icons[1].x-2, icons[1].y-2, 84, 80); icons[1].selected = !icons[1].selected;
  cairo_clip(cr); paint_desktop(cr, 1920, 1080); cairo_destroy(cr);
  render(full, FALSE, 0, 0); assert_same(partial, full); rubber_select = FALSE;
 }
 cairo_surface_destroy(partial); cairo_surface_destroy(full);
 g_print("PASS: incremental drag/overlap/selection pixels match full redraws in all wallpaper modes at %d%% scale\n", scale*100);
}
int main(int argc, char **argv)
{
 luna_init(); load_wallpaper(); fixture();
 verify_damage(1); verify_damage(2); wall_mode = WALL_STRETCH; icons[1].selected = TRUE;
 cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1920, 1080);
 render(surface, FALSE, 0, 0);
 gint64 start = g_get_monotonic_time();
 for (int i = 0; i < 12; i++) render(surface, FALSE, 0, 0);
 double full = (g_get_monotonic_time()-start)/12000.;
 start = g_get_monotonic_time();
 for (int i = 0; i < 100; i++) render(surface, TRUE, icons[0].x, icons[0].y);
 double partial = (g_get_monotonic_time()-start)/100000.;
 printf("1920x1080, 32 selected icons: full repaint %.3f ms/frame; small damage %.3f ms/frame\n", full, partial);
 cairo_surface_destroy(surface);
 for (guint i = 0; i < icon_count; i++) free_icon(&icons[i]);
 g_free(icons); g_clear_object(&wallpaper);
 if (wallpaper_cache) cairo_surface_destroy(wallpaper_cache);
 if (wallpaper_surface) cairo_surface_destroy(wallpaper_surface);
 g_free(wallpaper_path); luna_finish(); return 0;
}
