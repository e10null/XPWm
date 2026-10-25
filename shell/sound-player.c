/* Play one named event through the same optional backend as the shell. */
#include <glib.h>
#include "../shared/sounds.h"
int main(int argc, char **argv)
{
	static const struct { const char *name; enum XpwmSound sound; } events[] = {
		{"startup",XPWM_SOUND_STARTUP}, {"logon",XPWM_SOUND_LOGON}, {"logoff",XPWM_SOUND_LOGOFF},
		{"shutdown",XPWM_SOUND_SHUTDOWN}, {"menu",XPWM_SOUND_MENU}, {"notify",XPWM_SOUND_NOTIFY},
		{"error",XPWM_SOUND_ERROR}, {"minimize",XPWM_SOUND_MINIMIZE}, {"restore",XPWM_SOUND_RESTORE},
		{"navigation",XPWM_SOUND_NAVIGATION}, {"warning",XPWM_SOUND_WARNING},
		{"critical",XPWM_SOUND_CRITICAL}, {"default",XPWM_SOUND_DEFAULT},
  {"device-connect",XPWM_SOUND_DEVICE_CONNECT}, {"device-disconnect",XPWM_SOUND_DEVICE_DISCONNECT},
	};
	for (guint i=0; i<G_N_ELEMENTS(events); i++) {
		if (argc == 2 && !g_strcmp0(argv[1], "--list")) g_print("%s\n", events[i].name);
		else if (argc == 2 && !g_strcmp0(argv[1], events[i].name)) { xp_sound_play(events[i].sound); return 0; }
	}
	if (argc == 2 && !g_strcmp0(argv[1], "--list")) return 0;
	g_printerr("Usage: xpwm-sound EVENT | --list\n"); return 2;
}
