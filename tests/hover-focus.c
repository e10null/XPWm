#define main dwl_original_main
#include "../compositor/dwl.c"
#undef main
#include <assert.h>

static struct wl_event_source *test_timer;
static Client *first, *second;
static int phase, attempts;

static void move_to(double x, double y, uint32_t time)
{
	wlr_cursor_warp_closest(cursor, NULL, x, y);
	motionnotify(time, NULL, 0, 0, 0, 0);
}

static void save_focus(const char *value)
{
	char *dir = g_build_filename(g_get_user_config_dir(), "xpwm", NULL);
	char *path = g_build_filename(dir, "taskbar.ini", NULL);
	char *contents = g_strdup_printf("[Windows]\nFocusFollowsMouse=%s\n", value);
	assert(g_mkdir_with_parents(dir, 0700) == 0);
	assert(g_file_set_contents(path, contents, -1, NULL));
	g_free(dir); g_free(path); g_free(contents);
}

static int step(void *data)
{
	Client *c, *hit = NULL;
	(void)data;
	assert(++attempts < 100);
	if (!first && wl_list_length(&clients) >= 2) {
		first = wl_container_of(clients.next, c, link);
		second = wl_container_of(first->link.next, c, link);
	}
	if (!first || !second || first->resize || second->resize) goto again;
	switch (phase) {
	case 0:
		assert(!hoverfocus);
		resize(first, (struct wlr_box){100, 100, 500, 300}, 0);
		resize(second, (struct wlr_box){350, 180, 500, 300}, 0);
		focusclient(second, 1);
		move_to(150, 150, 100); assert(focustop(selmon) == second);
		save_focus("true"); phase++; break;
	case 1:
		if (!hoverfocus) break; /* Settings reload on the real compositor timer. */
		move_to(150, 150, 0); assert(focustop(selmon) == second);
		move_to(150, 150, 101); assert(focustop(selmon) == first);
		xytonode(400, 250, NULL, &hit, NULL, NULL, NULL);
		assert(hit == second); /* Focus did not raise Alpha over Beta. */
		focusclient(second, 1);
		move_to(150, 112, 102); assert(focustop(selmon) == first); /* Caption. */
		locked = 1;
		move_to(700, 250, 103); assert(focustop(selmon) == first);
		locked = 0;
		cursor_mode = CurPressed; seat->pointer_state.button_count = 1;
		pointerfocus(second, client_surface(second), 10, 40, 104);
		assert(focustop(selmon) == first);
		cursor_mode = CurNormal; seat->pointer_state.button_count = 0;
		move_to(20, 20, 105); assert(focustop(selmon) == first);
		save_focus("false"); phase++; break;
	case 2:
		if (hoverfocus) break;
		move_to(700, 250, 106); assert(focustop(selmon) == first);
		save_focus("invalid"); phase++; break;
	case 3:
		reloadfocussettings(); assert(!hoverfocus);
		fprintf(stderr, "PASS: live hover-focus setting, click-focus default, captions, unchanged stacking, locks and drag protection\n");
		phase++; wl_display_terminate(dpy); return 0;
	}
again:
	wl_event_source_timer_update(test_timer, 100); return 0;
}

int main(void)
{
	setup();
	wlr_seat_set_capabilities(seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
	test_timer = wl_event_loop_add_timer(event_loop, step, NULL);
	wl_event_source_timer_update(test_timer, 100);
	run("sh tests/hover-focus.sh client");
	assert(phase == 4);
	wl_event_source_remove(test_timer); cleanup(); return 0;
}
