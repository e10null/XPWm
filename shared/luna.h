#ifndef XPWM_LUNA_H
#define XPWM_LUNA_H
#include <cairo.h>
int luna_init(void);
int xp_icon(cairo_t *cr, int resource, int size, double x, double y);
int xp_start_flag(cairo_t *cr, double x, double y);
/* Draw a provenance-preserving derivative from assets/local/xp-bitmaps. */
int xp_bitmap(cairo_t *cr, const char *relative, double x, double y);
/* State is zero-based; sizing margins are read from the local theme manifest. */
int luna_draw(cairo_t *cr, const char *role, int state, double x, double y, double width, double height);
void luna_finish(void);
#endif
