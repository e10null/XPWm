/* Real Wayland pointer events and popup lifecycle, without host input devices. */
#define main dwl_main
#include "../compositor/dwl.c"
#undef main
#include <assert.h>

static struct wl_event_source *timer;
static int phase, attempts;

static Client *find_title(const char *title)
{
	Client *c;
	wl_list_for_each(c, &clients, link)
		if (!strcmp(client_get_title(c) ? client_get_title(c) : "", title)) return c;
	return NULL;
}

static void send_key(uint32_t code)
{
	struct timespec now;
	struct wlr_keyboard_key_event event = {.keycode = code,
		.state = WL_KEYBOARD_KEY_STATE_PRESSED};
	clock_gettime(CLOCK_MONOTONIC, &now);
	event.time_msec = (uint32_t)(now.tv_sec * 1000 + now.tv_nsec / 1000000);
	keypress(&kb_group->key, &event);
	event.state = WL_KEYBOARD_KEY_STATE_RELEASED;
	keypress(&kb_group->key, &event);
}

static int popup_count(void)
{
	struct wlr_xdg_client *xc;
	struct wlr_xdg_surface *surface;
	int count = 0;
	wl_list_for_each(xc, &xdg_shell->clients, link)
		wl_list_for_each(surface, &xc->surfaces, link)
			if (surface->role == WLR_XDG_SURFACE_ROLE_POPUP && surface->surface->mapped) count++;
	return count;
}

static void click_at(double x, double y)
{
	struct timespec now;
	struct wlr_pointer_button_event event = {.button = BTN_LEFT,
		.state = WL_POINTER_BUTTON_STATE_PRESSED};
	clock_gettime(CLOCK_MONOTONIC, &now);
	event.time_msec = (uint32_t)(now.tv_sec * 1000 + now.tv_nsec / 1000000);
	wlr_cursor_warp_closest(cursor, NULL, x, y);
	motionnotify(event.time_msec, NULL, 0, 0, 0, 0);
	buttonpress(NULL, &event);
	event.state = WL_POINTER_BUTTON_STATE_RELEASED;
	buttonpress(NULL, &event);
	wlr_seat_pointer_notify_frame(seat);
}

static int step(void *data)
{
	if (++attempts >= 44) {
		fprintf(stderr, "Shell UI timeout at phase %d, popup count %d\n", phase, popup_count());
		abort();
	}
	if (!selmon || selmon->w.height != selmon->m.height - 30) goto again;
	switch (phase) {
	case 0:
		if (!find_title("XPWm decoration test")) goto again;
		click_at(40, selmon->m.height-15); phase++; break;
	case 1:
		if (!popup_count()) goto again;
		if (getenv("XPWM_MENU_SCREENSHOT"))
			spawn(&(Arg)SHCMD("grim \"$XPWM_MENU_SCREENSHOT\""));
		phase++; break;
	case 2:
		click_at(selmon->m.width-100, 100); phase++; break;
	case 3:
		if (popup_count()) goto again;
		assert(!exclusive_focus);
		assert(seat->keyboard_state.focused_surface == client_surface(find_title("XPWm decoration test")));
		click_at(40, selmon->m.height-15); phase++; break;
	case 4:
		if (!popup_count()) goto again;
		/* Run is in the final 30px row, above the 40px footer. */
		click_at(260, selmon->m.height-30-40-15); phase++; break;
	case 5:
		if (!find_title("Run")) goto again;
		assert(!popup_count());
		assert(seat->keyboard_state.focused_surface == client_surface(find_title("Run")));
		send_key(KEY_ESC); phase++; break;
	case 6:
		if (find_title("Run")) goto again;
		assert(seat->keyboard_state.focused_surface == client_surface(find_title("XPWm decoration test")));
		click_at(40, selmon->m.height-15); phase++; break;
	case 7:
		if (!popup_count()) goto again;
		assert(exclusive_focus);
		assert(seat->keyboard_state.focused_surface != client_surface(find_title("XPWm decoration test")));
		send_key(KEY_ESC); phase++; break;
	case 8:
		if (popup_count()) goto again;
		assert(!exclusive_focus);
		assert(seat->keyboard_state.focused_surface == client_surface(find_title("XPWm decoration test")));
		click_at(40, selmon->m.height-15); phase++; break;
	case 9:
		if (!popup_count()) goto again;
		send_key(KEY_END); phase++; break;
	case 10:
		send_key(KEY_RIGHT); phase++; break;
	case 11:
		if (popup_count() != 2) goto again;
		send_key(KEY_LEFT); phase++; break;
	case 12:
		if (popup_count() != 1) goto again;
		send_key(KEY_HOME); phase++; break;
	case 13:
		send_key(KEY_RIGHT); phase++; break;
	case 14:
		send_key(KEY_END); phase++; break;
	case 15:
		send_key(KEY_ENTER); phase++; break;
	case 16:
		if (!find_title("Run")) goto again;
		assert(!popup_count());
		send_key(KEY_ESC); phase++; break;
	case 17:
		if (find_title("Run")) goto again;
		assert(!exclusive_focus);
		assert(seat->keyboard_state.focused_surface == client_surface(find_title("XPWm decoration test")));
		click_at(40, selmon->m.height-15); phase++; break;
	case 18:
		if (!popup_count()) goto again;
		send_key(KEY_END); phase++; break;
	case 19:
		send_key(KEY_RIGHT); phase++; break;
	case 20:
		if (popup_count() != 2) goto again;
		send_key(KEY_HOME); phase++; break;
	case 21:
		assert(popup_count() == 2);
		send_key(KEY_DOWN); phase++; break;
	case 22:
		assert(popup_count() == 2);
		send_key(KEY_UP); phase++; break;
	case 23:
		assert(popup_count() == 2);
		send_key(KEY_DOWN); phase++; break;
	case 24:
		assert(popup_count() == 2);
		send_key(KEY_ENTER); phase++; break;
	case 25: {
		char path[1024], name[32] = {0};
		FILE *file;
		snprintf(path, sizeof(path), "%s/launched", getenv("XDG_RUNTIME_DIR"));
		if (!(file = fopen(path, "r"))) goto again;
		assert(fgets(name, sizeof(name), file)); fclose(file);
		assert(!strcmp(name, "Beta"));
		assert(!popup_count());
		assert(!exclusive_focus);
		fprintf(stderr, "PASS: Start keyboard/pointer, submenu traversal, safe Beta launch, Run, Escape and focus restoration\n");
		phase++;
		wl_display_terminate(dpy);
		return 0;
	}
	}
again:
	wl_event_source_timer_update(timer, 600);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 3 && !strcmp(argv[1], "--fixture")) {
		char path[1024];
		FILE *file;
		snprintf(path, sizeof(path), "%s/launched", getenv("XDG_RUNTIME_DIR"));
		assert((file = fopen(path, "w")));
		fputs(argv[2], file); fclose(file);
		return 0;
	}
	setup();
	wlr_seat_set_capabilities(seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
	timer = wl_event_loop_add_timer(event_loop, step, NULL);
	wl_event_source_timer_update(timer, 300);
	run("sh tests/window-client.sh");
	assert(phase == 26);
	wl_event_source_remove(timer);
	cleanup();
	return 0;
}
