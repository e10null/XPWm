#define main dwl_original_main
#include "../compositor/dwl.c"
#undef main
#include <wlr/interfaces/wlr_keyboard.h>
#include <assert.h>

static struct wl_event_source *test_timer;
static Client *application;
static int phase, attempts, leaked_super, wait_steps;

static void protocol_event(void *data, enum wl_protocol_logger_type direction,
		const struct wl_protocol_logger_message *message)
{
	(void)data;
	if (!application || direction != WL_PROTOCOL_LOGGER_EVENT
			|| strcmp(wl_resource_get_class(message->resource), "wl_keyboard")
			|| strcmp(message->message->name, "key")
			|| wl_resource_get_client(message->resource) != wl_resource_get_client(client_surface(application)->resource)) return;
	if (message->arguments[2].u == KEY_LEFTMETA || message->arguments[2].u == KEY_RIGHTMETA) leaked_super++;
}

static void key(uint32_t code, enum wl_keyboard_key_state state)
{
	struct wlr_keyboard_key_event event = {.time_msec=1000, .keycode=code, .update_state=true, .state=state};
	wlr_keyboard_notify_key(&kb_group->wlr_group->keyboard, &event);
}

static void tap(uint32_t code)
{
	key(code, WL_KEYBOARD_KEY_STATE_PRESSED); key(code, WL_KEYBOARD_KEY_STATE_RELEASED);
}

static int state_is(const char *expected)
{
	char *path = g_build_filename(getenv("XDG_RUNTIME_DIR"), "start-key-state", NULL), *value = NULL;
	int matches;
	g_file_get_contents(path, &value, NULL, NULL);
	matches = !g_strcmp0(value, expected);
	g_free(path); g_free(value); return matches;
}

static int step(void *data)
{
	Monitor *m; LayerSurface *bar = NULL, *l; Client *c;
	(void)data; assert(++attempts < 130);
	wl_list_for_each(c, &clients, link)
		if (!strcmp(client_get_appid(c), "xpwm-start-test")) application = c;
	wl_list_for_each(m, &mons, link)
		for (int layer = 0; layer < 4; layer++) wl_list_for_each(l, &m->layers[layer], link)
			if (l->mapped && !strcmp(l->layer_surface->namespace, "xpwm-taskbar")) bar = l;
	if (!application || !bar) goto again;
	switch (phase) {
	case 0:
		if (bar->geom.y != bar->mon->m.y + bar->mon->m.height-2) break;
		focusclient(application, 1);
		wlr_cursor_warp_closest(cursor, NULL, application->geom.x+60, application->geom.y+60);
		motionnotify(0, NULL, 0, 0, 0, 0);
		key(KEY_LEFTMETA, WL_KEYBOARD_KEY_STATE_PRESSED);
		assert(!exclusive_focus && seat->keyboard_state.focused_surface == client_surface(application));
		key(KEY_LEFTMETA, WL_KEYBOARD_KEY_STATE_RELEASED); phase++; break;
	case 1:
		if (!state_is("open") || exclusive_focus != bar) break;
		assert(bar->geom.y == bar->mon->m.y + bar->mon->m.height-30);
		tap(KEY_RIGHTMETA); phase++; break;
	case 2:
		if (!state_is("closed") || exclusive_focus) break;
		assert(seat->keyboard_state.focused_surface == client_surface(application));
		key(KEY_LEFTMETA, WL_KEYBOARD_KEY_STATE_PRESSED);
		tap(KEY_E);
		key(KEY_LEFTMETA, WL_KEYBOARD_KEY_STATE_RELEASED); phase++; break;
	case 3: {
		char *path = g_build_filename(getenv("XDG_RUNTIME_DIR"), "chord-ran", NULL);
		int ran = g_file_test(path, G_FILE_TEST_EXISTS);
		struct wlr_pointer_button_event button = {.button=BTN_LEFT, .state=WL_POINTER_BUTTON_STATE_PRESSED, .time_msec=1001};
		g_free(path);
		if (!ran || wait_steps++ < 3) break;
		assert(state_is("closed") && !exclusive_focus);
		key(KEY_LEFTMETA, WL_KEYBOARD_KEY_STATE_PRESSED);
		buttonpress(NULL, &button);
		assert(!start_candidate);
		button.state = WL_POINTER_BUTTON_STATE_RELEASED; buttonpress(NULL, &button);
		key(KEY_LEFTMETA, WL_KEYBOARD_KEY_STATE_RELEASED);
		assert(state_is("closed"));
		key(KEY_LEFTMETA, WL_KEYBOARD_KEY_STATE_PRESSED);
		start_pressed_at=1000-251;
		key(KEY_LEFTMETA, WL_KEYBOARD_KEY_STATE_RELEASED);
		assert(state_is("closed") && !start_candidate);
		key(KEY_RIGHTMETA, WL_KEYBOARD_KEY_STATE_PRESSED);
		start_pressed_at=1000-2000;
		key(KEY_RIGHTMETA, WL_KEYBOARD_KEY_STATE_RELEASED);
		assert(state_is("closed"));
		locked = 1; tap(KEY_LEFTMETA); locked = 0;
		assert(state_is("closed"));
		tap(KEY_LEFTMETA); phase++; break;
	}
	case 4:
		if (!state_is("open") || exclusive_focus != bar) break;
		tap(KEY_ESC); phase++; break;
	case 5:
		if (exclusive_focus) break;
		assert(!leaked_super && !consumed[KEY_LEFTMETA] && !consumed[KEY_RIGHTMETA]);
		assert(seat->keyboard_state.focused_surface == client_surface(application));
		fprintf(stderr, "PASS: Windows-key tap opens/closes Start, auto-hide reveal, Super+E preserved, lock guard, Escape and application focus restoration\n");
		phase++; wl_display_terminate(dpy); return 0;
	}
again:
	wl_event_source_timer_update(test_timer, 100); return 0;
}

int main(void)
{
	struct wl_protocol_logger *logger;
	setup();
	logger = wl_display_add_protocol_logger(dpy, protocol_event, NULL);
	wlr_seat_set_capabilities(seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
	test_timer = wl_event_loop_add_timer(event_loop, step, NULL);
	wl_event_source_timer_update(test_timer, 100);
	run("sh tests/start-key.sh client"); assert(phase == 6);
	wl_event_source_remove(test_timer); wl_protocol_logger_destroy(logger); cleanup(); return 0;
}
