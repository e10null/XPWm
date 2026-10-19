/* Drive the actual Welcome/power/logoff layer surfaces through Wayland input. */
#define main dwl_main
#include "../compositor/dwl.c"
#undef main
#include <assert.h>
static int phase, attempts;
static struct wl_event_source *timer;

static LayerSurface *find_layer(const char *name)
{
	LayerSurface *l;
	if (!selmon) return NULL;
	wl_list_for_each(l, &selmon->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], link)
		if (l->mapped && !strcmp(l->layer_surface->namespace, name)) return l;
	return NULL;
}
static void click(double x, double y)
{
	struct timespec now;
	struct wlr_pointer_button_event event = {.button=BTN_LEFT, .state=WL_POINTER_BUTTON_STATE_PRESSED};
	clock_gettime(CLOCK_MONOTONIC, &now);
	event.time_msec = (uint32_t)(now.tv_sec*1000 + now.tv_nsec/1000000);
	wlr_cursor_warp_closest(cursor, NULL, x, y); motionnotify(event.time_msec, NULL, 0, 0, 0, 0);
	buttonpress(NULL, &event); event.state=WL_POINTER_BUTTON_STATE_RELEASED; buttonpress(NULL, &event);
	wlr_seat_pointer_notify_frame(seat);
}
static void escape_key(void)
{
	struct wlr_keyboard_key_event event = {.keycode=KEY_ESC, .state=WL_KEYBOARD_KEY_STATE_PRESSED};
	keypress(&kb_group->key, &event); event.state=WL_KEYBOARD_KEY_STATE_RELEASED; keypress(&kb_group->key, &event);
}
static int step(void *data)
{
	LayerSurface *l = find_layer("xpwm-logon");
	if (++attempts > 70) { fprintf(stderr, "Welcome UI timeout at phase %d\n", phase); abort(); }
	switch (phase) {
	case 0:
		if (!l) break;
		assert(l->layer_surface->current.actual_width == (uint32_t)selmon->m.width);
		assert(l->layer_surface->current.actual_height == (uint32_t)selmon->m.height);
		assert(exclusive_focus == l);
		phase++; break;
	case 1:
		if (getenv("XPWM_LOGON_SCREENSHOT")) spawn(&(Arg)SHCMD("grim \"$XPWM_LOGON_SCREENSHOT\""));
		escape_key(); phase++; break;
	case 2:
		assert(l); click(65, selmon->m.height-40); phase++; break;
	case 3: {
		LayerSurface *panel = find_layer("xpwm-session-panel");
		if (!panel) break;
		assert(exclusive_focus == panel);
		click(panel->geom.x+261, panel->geom.y+177); phase++; break;
	}
	case 4:
		if (find_layer("xpwm-session-panel")) break;
		assert(l && exclusive_focus == l);
		click(selmon->m.width/2+70, (80+selmon->m.height-74)/2-10); phase++; break;
	case 5:
		assert(l);
		if (getenv("XPWM_WELCOME_SCREENSHOT")) spawn(&(Arg)SHCMD("grim \"$XPWM_WELCOME_SCREENSHOT\""));
		phase++; break;
	case 6: {
		char value[128]={0}; FILE *file;
		if (l) break;
		file = fopen(getenv("XPWM_SESSION_MARKER"), "r"); assert(file);
		assert(fgets(value, sizeof(value), file)); fclose(file);
		assert(strstr(value, "accepted user"));
		spawn(&(Arg)SHCMD("XPWM_LOGOFF_DELAY_MS=700 ./shell/xpwm-logoff")); phase++; break;
	}
	case 7:
		if (!find_layer("xpwm-logoff")) break;
		phase++; break;
	case 8:
		if (getenv("XPWM_LOGOFF_SCREENSHOT")) spawn(&(Arg)SHCMD("grim \"$XPWM_LOGOFF_SCREENSHOT\""));
		phase++; break;
	case 9: {
		char value[32]={0}; FILE *file;
		if (find_layer("xpwm-logoff")) break;
		file = fopen(getenv("XPWM_SESSION_MARKER"), "r"); assert(file);
		assert(fgets(value, sizeof(value), file)); fclose(file); assert(!strcmp(value,"logoff"));
		fprintf(stderr,"PASS: Welcome geometry, Escape, power confirmation/cancel, restored focus, account click, welcome/logoff transitions\n");
		phase++; wl_display_terminate(dpy); return 0;
	}
	}
	wl_event_source_timer_update(timer, 150); return 0;
}
int main(void)
{
	assert(getenv("XPWM_SESSION_MARKER"));
	setenv("XPWM_SESSION_DRY_RUN","1",1); setenv("XPWM_SOUND_MUTE","1",1);
	unsetenv("XPWM_LOGON_AUTO"); unsetenv("XPWM_LOGON_PASSWORD_PROMPT");
	setup(); wlr_seat_set_capabilities(seat, WL_SEAT_CAPABILITY_POINTER|WL_SEAT_CAPABILITY_KEYBOARD);
	timer=wl_event_loop_add_timer(event_loop,step,NULL); wl_event_source_timer_update(timer,150);
	run("./shell/xpwm-logon"); assert(phase==10); wl_event_source_remove(timer); cleanup(); return 0;
}
