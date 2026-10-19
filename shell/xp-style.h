#ifndef XPWM_XP_STYLE_H
#define XPWM_XP_STYLE_H
#include <gtk/gtk.h>
/* Scoped to shell-owned widgets; never changes the host GTK configuration. */
void xp_style_apply(GtkWidget *root);
#endif
