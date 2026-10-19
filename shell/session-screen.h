#ifndef XPWM_SESSION_SCREEN_H
#define XPWM_SESSION_SCREEN_H

enum XpwmSessionScreenMode {
	XPWM_SCREEN_LOGON,
	XPWM_SCREEN_LOGOFF,
};

/* Run a full-screen layer-shell XP Welcome/logoff surface. */
int xpwm_session_screen_run(enum XpwmSessionScreenMode mode, int argc, char **argv);

#endif
