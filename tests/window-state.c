/* Integration harness: real wlroots backend + real foot client, compositor input. */
#define main dwl_main
#include "../compositor/dwl.c"
#undef main
#include <assert.h>

static struct wl_event_source *test_timer;
static int phase, attempts;
static struct wlr_box initial;

static void
click(Client *c, int button, int cancel)
{
	struct wlr_pointer_button_event event = {
		.button = BTN_LEFT, .state = WL_POINTER_BUTTON_STATE_PRESSED,
		.time_msec = 1000 + (uint32_t)phase * 1000,
	};
	double x = c->geom.x + c->geom.width - c->bw - 3
			- (4 - button) * (XP_BUTTON + XP_GAP) + XP_BUTTON / 2;
	double y = c->geom.y + c->bw + 12;
	wlr_cursor_warp_closest(cursor, NULL, x, y);
	buttonpress(NULL, &event);
	if (cancel)
		wlr_cursor_warp_closest(cursor, NULL, c->geom.x + 40, c->geom.y + 45);
	event.state = WL_POINTER_BUTTON_STATE_RELEASED;
	buttonpress(NULL, &event);
}

static int
step(void *data)
{
	Client *c = wl_list_empty(&clients) ? NULL : wl_container_of(clients.next, c, link);
	Client *hit = NULL;
	struct wlr_surface *surface = NULL;
	struct wlr_pointer_button_event event = {.button = BTN_LEFT,
		.state = WL_POINTER_BUTTON_STATE_PRESSED, .time_msec = 20000};
	(void)data;
	assert(++attempts < 80);
	if ((!c || c->resize) && phase < 8)
		goto again;
	switch (phase++) {
	case 0:
		if (c->mon->w.height != c->mon->m.height - 30) { phase--; break; }
		assert(c->caption && c->caption->buffer);
		initial = (struct wlr_box){100, 100, 480, 320};
		resize(c, initial, 0);
		break;
	case 1:
		focusclient(NULL, 0);
		assert(!c->caption_active);
		focusclient(c, 1);
		assert(c->caption_active);
		assert(c->scene_surface->node.y == (int)c->bw + XP_CAPTION);
		xytonode(c->geom.x + 40, c->geom.y + 12, &surface, &hit, NULL, NULL, NULL);
		assert(hit == c && !surface); /* Caption buffer is not a client surface. */
		click(c, 2, 1);
		assert(!c->maximized); /* Release outside cancels. */
		click(c, 2, 0);
		assert(c->maximized && wlr_box_equal(&c->geom, &c->mon->w));
		break;
	case 2:
		assert(c->surface.xdg->toplevel->current.maximized);
		setfullscreen(c, 1);
		assert(!c->caption->node.enabled);
		setfullscreen(c, 0);
		assert(c->caption->node.enabled && c->maximized);
		click(c, 2, 0);
		assert(!c->maximized && wlr_box_equal(&c->geom, &initial));
		break;
	case 3:
		click(c, 1, 0);
		assert(c->minimized && !c->scene->node.enabled && !focustop(selmon));
		focusstack(&(Arg){.i = 1});
		assert(!c->minimized && c->scene->node.enabled && focustop(selmon) == c);
		break;
	case 4:
		wlr_cursor_warp_closest(cursor, NULL, c->geom.x, c->geom.y + 80);
		buttonpress(NULL, &event);
		assert(cursor_mode == CurResize && resize_edges == WLR_EDGE_LEFT);
		motionnotify(21000, NULL, -20, 0, -20, 0);
		event.state = WL_POINTER_BUTTON_STATE_RELEASED;
		buttonpress(NULL, &event);
		assert(c->geom.x == initial.x - 20 && c->geom.width == initial.width + 20);
		break;
	case 5:
		wlr_cursor_warp_closest(cursor, NULL, c->geom.x + 40, c->geom.y + 12);
		buttonpress(NULL, &event);
		assert(cursor_mode == CurMove);
		motionnotify(22000, NULL, 30, 30, 30, 30);
		event.state = WL_POINTER_BUTTON_STATE_RELEASED;
		buttonpress(NULL, &event);
		assert(c->geom.x == initial.x + 10 && c->geom.y == initial.y + 30);
		if (getenv("XPWM_TEST_SCREENSHOT"))
			spawn(&(Arg)SHCMD("grim \"$XPWM_TEST_SCREENSHOT\""));
		break;
	case 6:
		/* A dragged floating client must tile, then regain its saved geometry. */
		initial = c->geom;
		xpallfloat(&(Arg){0});
		assert(selmon->lt[selmon->sellt]->arrange && !c->isfloating);
		assert(wlr_box_equal(&c->geom, &c->mon->w));
		xpallfloat(&(Arg){0});
		assert(!selmon->lt[selmon->sellt]->arrange && wlr_box_equal(&initial, &c->geom));
		locked = 1;
		wl_signal_emit_mutable(&c->foreign->events.request_minimize,
				&(struct wlr_foreign_toplevel_handle_v1_minimized_event){.minimized = true});
		assert(!c->minimized);
		locked = 0;
		break;
	case 7:
		click(c, 3, 0);
		break;
	case 8:
		if (c) { phase--; break; }
		assert(!caption_pressed && !caption_hover && !grabc);
		fprintf(stderr, "PASS: decorations, window states, taskbar work area, WM/Floating modes, locked request rejection, close\n");
		wl_display_terminate(dpy);
		return 0;
	}
again:
	wl_event_source_timer_update(test_timer, 150);
	return 0;
}

int
main(void)
{
	setup();
	test_timer = wl_event_loop_add_timer(event_loop, step, NULL);
	wl_event_source_timer_update(test_timer, 150);
	run("sh tests/window-client.sh");
	assert(phase == 9);
	wl_event_source_remove(test_timer);
	cleanup();
	return 0;
}
