/* Optional XP WAV playback with no mandatory audio-library dependency. */
#include <glib.h>
#include <string.h>
#include "sounds.h"

typedef struct {
	enum XpwmSound id;
	const char *file;
} Sound;

static const Sound sound_table[] = {
	{XPWM_SOUND_STARTUP, "startup.wav"},
	{XPWM_SOUND_LOGON, "logon.wav"},
	{XPWM_SOUND_LOGOFF, "logoff.wav"},
	{XPWM_SOUND_SHUTDOWN, "shutdown.wav"},
	{XPWM_SOUND_MENU, "menu.wav"},
	{XPWM_SOUND_NOTIFY, "notify.wav"},
	{XPWM_SOUND_ERROR, "error.wav"},
	{XPWM_SOUND_MINIMIZE, "minimize.wav"},
	{XPWM_SOUND_RESTORE, "restore.wav"},
	{XPWM_SOUND_NAVIGATION, "navigation.wav"},
	{XPWM_SOUND_WARNING, "warning.wav"},
	{XPWM_SOUND_CRITICAL, "critical.wav"},
	{XPWM_SOUND_DEFAULT, "default.wav"},
 {XPWM_SOUND_DEVICE_CONNECT, "device-connect.wav"},
 {XPWM_SOUND_DEVICE_DISCONNECT, "device-disconnect.wav"},
};

static const char *sound_file(enum XpwmSound sound)
{
	for (guint i = 0; i < G_N_ELEMENTS(sound_table); i++)
		if (sound_table[i].id == sound) return sound_table[i].file;
	return NULL;
}

static char *find_sound(const char *file)
{
	const char *override = g_getenv("XPWM_SOUND_DIR");
	const char *root = g_getenv("XPWM_ASSET_ROOT");
	char *exe, *dir, *project, *candidate;
	const char *relative[] = {
		"assets/local/sounds-xp",
		"assets/local/sounds",
		"assets/local/xp-sounds",
	};

	if (override && *override) {
		candidate = g_build_filename(override, file, NULL);
		if (g_file_test(candidate, G_FILE_TEST_IS_REGULAR)) return candidate;
		g_free(candidate);
	}
	if (root && *root) {
		for (guint i = 0; i < G_N_ELEMENTS(relative); i++) {
			candidate = g_build_filename(root, relative[i], file, NULL);
			if (g_file_test(candidate, G_FILE_TEST_IS_REGULAR)) return candidate;
			g_free(candidate);
		}
	}
	for (guint i = 0; i < G_N_ELEMENTS(relative); i++) {
		candidate = g_build_filename(relative[i], file, NULL);
		if (g_file_test(candidate, G_FILE_TEST_IS_REGULAR)) return candidate;
		g_free(candidate);
	}
	/* Shell binaries are normally in <project>/shell/. */
	exe = g_file_read_link("/proc/self/exe", NULL);
	if (!exe) return NULL;
	dir = g_path_get_dirname(exe);
	project = g_canonicalize_filename("..", dir);
	for (guint i = 0; i < G_N_ELEMENTS(relative); i++) {
		candidate = g_build_filename(project, relative[i], file, NULL);
		if (g_file_test(candidate, G_FILE_TEST_IS_REGULAR)) {
			g_free(project); g_free(dir); g_free(exe); return candidate;
		}
		g_free(candidate);
	}
	g_free(project); g_free(dir); g_free(exe);
	return NULL;
}

void xp_sound_play(enum XpwmSound sound)
{
	const char *file = sound_file(sound);
	const char *requested = g_getenv("XPWM_SOUND_PLAYER");
	const char *players[] = {"pw-play", "paplay", "aplay"};
	char *path, *player = NULL;
	GError *error = NULL;
	const char *argv[3];

	if (!file || g_getenv("XPWM_SOUND_MUTE")) return;
	path = find_sound(file);
	if (!path) return;
	if (requested && *requested)
		player = g_find_program_in_path(requested);
	else {
		for (guint i = 0; i < G_N_ELEMENTS(players); i++)
			if ((player = g_find_program_in_path(players[i]))) break;
	}
	if (!player) { g_free(path); return; }
	argv[0] = player; argv[1] = path; argv[2] = NULL;
	if (!g_spawn_async(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH,
		NULL, NULL, NULL, &error)) {
		g_printerr("XPWm: unable to play %s: %s\n", file, error->message);
		g_clear_error(&error);
	}
	g_free(player); g_free(path);
}
