#ifndef XPWM_SESSION_DIALOG_H
#define XPWM_SESSION_DIALOG_H
#include <gtk/gtk.h>
enum XpwmSessionResponse {
	XPWM_SESSION_STANDBY = 1,
	XPWM_SESSION_POWEROFF,
	XPWM_SESSION_REBOOT,
	XPWM_SESSION_LOGOFF,
};
GtkWidget *session_dialog_open(void);
GtkWidget *logoff_dialog_open(void);
#endif
