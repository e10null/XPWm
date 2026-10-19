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
		assert(c->caption->buffer->width == c->geom.width);
		assert(c->caption->buffer->height == XP_CAPTION + (int)c->bw);
		assert(c->frame[0]->buffer && c->frame[1]->buffer && c->frame[2]->buffer);
		assert(c->geom.x >= c->mon->w.x && c->geom.y >= c->mon->w.y);
		assert(c->geom.x+c->geom.width <= c->mon->w.x+c->mon->w.width);
		assert(c->geom.y+c->geom.height <= c->mon->w.y+c->mon->w.height);
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
		assert(!c->frame[0]->node.enabled && !c->frame[1]->node.enabled && !c->frame[2]->node.enabled);
		setfullscreen(c, 0);
		assert(c->caption->node.enabled && c->maximized);
		assert(c->frame[0]->node.enabled && c->frame[1]->node.enabled && c->frame[2]->node.enabled);
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
		initial = c->geom;
		assert(keybinding(WLR_MODIFIER_ALT, XKB_KEY_space));
		assert(system_scene && system_target == c && !(system_enabled(c) & 1u));
		assert(keybinding(0, XKB_KEY_Up) && system_row == 5);
		assert(keybinding(0, XKB_KEY_Escape) && !system_scene);
		keybinding(WLR_MODIFIER_ALT, XKB_KEY_space); keybinding(0, XKB_KEY_m);
		assert(system_moving == c && cursor_mode == CurMove);
		keybinding(0, XKB_KEY_Right); keybinding(0, XKB_KEY_Down);
		assert(c->geom.x == initial.x+8 && c->geom.y == initial.y+8);
		keybinding(0, XKB_KEY_Escape);
		assert(!system_moving && !grabc && wlr_box_equal(&c->geom, &initial));
		break;
	case 8:
		keybinding(WLR_MODIFIER_ALT, XKB_KEY_space); keybinding(0, XKB_KEY_s);
		assert(system_moving == c && cursor_mode == CurResize);
		keybinding(0, XKB_KEY_Right); keybinding(WLR_MODIFIER_CTRL, XKB_KEY_Down);
		assert(c->geom.width == initial.width+8 && c->geom.height == initial.height+1);
		keybinding(0, XKB_KEY_Return); assert(!system_moving && !grabc);
		initial = c->geom;
		break;
	case 9:
		event.button = BTN_RIGHT;
		wlr_cursor_warp_closest(cursor, NULL, c->geom.x+100, c->geom.y+12);
		buttonpress(NULL, &event); event.state = WL_POINTER_BUTTON_STATE_RELEASED; buttonpress(NULL, &event);
		assert(system_scene && system_target == c);
		if (getenv("XPWM_SYSTEM_MENU_SCREENSHOT")) spawn(&(Arg)SHCMD("grim \"$XPWM_SYSTEM_MENU_SCREENSHOT\""));
		break;
	case 10:
		keybinding(0, XKB_KEY_x); assert(c->maximized && !system_scene);
		keybinding(WLR_MODIFIER_ALT, XKB_KEY_space);
		assert((system_enabled(c) & 1u) && !(system_enabled(c) & ((1u<<1)|(1u<<2)|(1u<<4))));
		keybinding(0, XKB_KEY_r); assert(!c->maximized && wlr_box_equal(&c->geom, &initial));
		wlr_cursor_warp_closest(cursor, NULL, c->geom.x+c->bw+12, c->geom.y+c->bw+12);
		buttonpress(NULL, &event); event.state = WL_POINTER_BUTTON_STATE_RELEASED; buttonpress(NULL, &event);
		assert(system_scene && system_target == c);
		break;
	case 11:
		wlr_cursor_warp_closest(cursor, NULL, system_scene->node.x+60, system_scene->node.y+3+40+10);
		buttonpress(NULL, &event); event.state = WL_POINTER_BUTTON_STATE_RELEASED; buttonpress(NULL, &event);
		assert(system_moving == c && system_move_kind == 2);
		motionnotify(23000, NULL, 16, 9, 16, 9);
		assert(c->geom.width == initial.width+16 && c->geom.height == initial.height+9);
		event.state = WL_POINTER_BUTTON_STATE_PRESSED; buttonpress(NULL, &event);
		event.state = WL_POINTER_BUTTON_STATE_RELEASED; buttonpress(NULL, &event);
		assert(!system_moving && !grabc);
		fprintf(stderr, "PASS: Alt+Space/title-bar/icon menu, disabled actions, keyboard Move cancellation, keyboard and pointer Size, maximize/restore\n");
		break;
	case 12:
		keybinding(WLR_MODIFIER_ALT, XKB_KEY_space); keybinding(0, XKB_KEY_c);
		break;
	case 13:
		if (c) { phase--; break; }
		assert(!caption_pressed && !caption_hover && !grabc && !system_scene && !system_target && !system_moving);
		fprintf(stderr, "PASS: decorations, window states, taskbar work area, WM/Floating modes, locked request rejection, system-menu close\n");
		wl_display_terminate(dpy); return 0;
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
	assert(phase == 14);
	wl_event_source_remove(test_timer);
	cleanup();
	return 0;
}
