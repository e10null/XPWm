#ifndef XPWM_SOUNDS_H
#define XPWM_SOUNDS_H

/* XP event sounds are optional local assets.  Playback is deliberately kept
 * outside the compositor: a shell helper can use the host's PipeWire,
 * PulseAudio, or ALSA command without adding an audio-library dependency. */
enum XpwmSound {
	XPWM_SOUND_STARTUP,
	XPWM_SOUND_LOGON,
	XPWM_SOUND_LOGOFF,
	XPWM_SOUND_SHUTDOWN,
	XPWM_SOUND_MENU,
	XPWM_SOUND_NOTIFY,
	XPWM_SOUND_ERROR,
	XPWM_SOUND_MINIMIZE,
	XPWM_SOUND_RESTORE,
	XPWM_SOUND_NAVIGATION,
	XPWM_SOUND_WARNING,
	XPWM_SOUND_CRITICAL,
	XPWM_SOUND_DEFAULT,
 XPWM_SOUND_DEVICE_CONNECT,
 XPWM_SOUND_DEVICE_DISCONNECT,
};

void xp_sound_play(enum XpwmSound sound);

#endif
