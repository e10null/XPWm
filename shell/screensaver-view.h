#ifndef XPWM_SCREENSAVER_VIEW_H
#define XPWM_SCREENSAVER_VIEW_H
#include <gtk/gtk.h>
#include "../shared/screensaver-config.h"
GtkWidget *xp_saver_view_new(const XpSaverConfig *config);
void xp_saver_view_configure(GtkWidget *view, const XpSaverConfig *config);
#endif
