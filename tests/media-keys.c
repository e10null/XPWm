#define main dwl_original_main
#include "../compositor/dwl.c"
#undef main
#include <assert.h>
#include <glib/gstdio.h>

int main(void)
{
	char *root = g_dir_make_tmp("xpwm media-XXXXXX", NULL);
	char *tools = g_build_filename(root, "tools", NULL);
	char *wpctl = g_build_filename(root, "wpctl", NULL);
	char *helper = g_build_filename(tools, "brightness.py", NULL);
	char *log = g_build_filename(root, "commands", NULL);
	char *path = g_strdup_printf("%s:%s", root, g_getenv("PATH"));
	const char *audio = "#!/bin/sh\nprintf '%s\\n' \"$*\" >> \"$XPWM_MEDIA_TEST_LOG\"\n";
	const char *backlight = "import os, sys\nwith open(os.environ['XPWM_MEDIA_TEST_LOG'], 'a') as f: f.write('brightness ' + sys.argv[1] + '\\n')\n";
	xkb_keysym_t symbols[] = {XKB_KEY_XF86AudioRaiseVolume, XKB_KEY_XF86AudioLowerVolume,
		XKB_KEY_XF86AudioMute, XKB_KEY_XF86AudioMicMute,
		XKB_KEY_XF86MonBrightnessDown, XKB_KEY_XF86MonBrightnessUp};
	uint32_t codes[] = {KEY_VOLUMEUP, KEY_VOLUMEDOWN, KEY_MUTE, KEY_MICMUTE,
		KEY_BRIGHTNESSDOWN, KEY_BRIGHTNESSUP};
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *map = xkb_keymap_new_from_names(context, &xkb_rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	struct xkb_state *state = xkb_state_new(map);
	assert(root && g_mkdir(tools, 0700) == 0);
	assert(g_file_set_contents(wpctl, audio, -1, NULL));
	assert(g_chmod(wpctl, 0700) == 0);
	assert(g_file_set_contents(helper, backlight, -1, NULL));
	g_setenv("PATH", path, TRUE); g_setenv("XPWM_PROJECT_ROOT", root, TRUE);
	g_setenv("XPWM_MEDIA_TEST_LOG", log, TRUE);
	/* Sentinel overlays would crash if they swallowed a media shortcut. */
	system_scene = (void *)1; switch_scene = (void *)1;
	for (size_t i = 0; i < LENGTH(symbols); i++) {
		int status;
		xkb_keysym_t actual = xkb_state_key_get_one_sym(state, codes[i] + 8);
		assert(actual == symbols[i]);
		assert(keybinding(0, actual) == 1);
		assert(waitpid(-1, &status, 0) > 0);
		assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	}
	char *contents = NULL;
	assert(g_file_get_contents(log, &contents, NULL, NULL));
	assert(strstr(contents, "set-mute @DEFAULT_AUDIO_SINK@ 0\nset-volume -l 1.0 @DEFAULT_AUDIO_SINK@ 5%+"));
	assert(strstr(contents, "set-volume -l 1.0 @DEFAULT_AUDIO_SINK@ 5%-"));
	assert(strstr(contents, "set-mute @DEFAULT_AUDIO_SOURCE@ toggle"));
	assert(strstr(contents, "brightness down\nbrightness up\n"));
	g_remove(log); g_remove(helper); g_remove(wpctl); g_rmdir(tools); g_rmdir(root);
	g_free(contents); g_free(root); g_free(tools); g_free(wpctl); g_free(helper); g_free(log); g_free(path);
	xkb_state_unref(state); xkb_keymap_unref(map); xkb_context_unref(context);
	puts("PASS: six physical media-key bindings execute with menus/Alt+Tab open and a project path containing spaces");
	return 0;
}
