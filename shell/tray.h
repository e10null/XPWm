#ifndef XPWM_TRAY_H
#define XPWM_TRAY_H
#include <gtk/gtk.h>
void xp_tray_init(GtkWidget *canvas);
void xp_tray_stop(void);
void xp_tray_set_clock_visible(gboolean visible);
gboolean xp_tray_popup_open(void);
int xp_tray_width(void);
int xp_tray_hit(int x);
int xp_tray_clock_x(void);
int xp_tray_clock_width(void);
void xp_tray_draw(cairo_t *cr, int x, int height);
const char *xp_tray_tooltip(int x);
void xp_tray_click(int x, guint button);
void xp_tray_double_click(int x);
gboolean xp_tray_scroll(int x, GdkScrollDirection direction);
#endif
