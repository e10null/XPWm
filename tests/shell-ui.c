#include <glib.h>
#undef MIN
#undef MAX
/* Real Wayland pointer events and popup lifecycle, without host input devices. */
#define main dwl_main
#include "../compositor/dwl.c"
#undef main
#include <assert.h>

static struct wl_event_source *timer;
static int phase, attempts, run_snapshot, volume_snapshot, properties_snapshot;

static Client *find_title(const char *title)
{
	Client *c;
	wl_list_for_each(c, &clients, link)
		if (!strcmp(client_get_title(c) ? client_get_title(c) : "", title)) return c;
	return NULL;
}

static int popup_count(void);

static LayerSurface *find_layer(const char *name)
{
	LayerSurface *l;
	if (!selmon) return NULL;
	wl_list_for_each(l, &selmon->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], link)
		if (l->mapped && !strcmp(l->layer_surface->namespace, name)) return l;
	return NULL;
}

static void check_panel(LayerSurface *panel)
{
	assert(panel->layer_surface->current.actual_width == 313);
	assert(panel->layer_surface->current.actual_height == 198);
	assert(panel == exclusive_focus);
	assert(find_layer("xpwm-session-shade"));
	assert(!popup_count());
	assert(seat->keyboard_state.focused_surface == panel->layer_surface->surface);
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

static void click_button_at(double x, double y, guint button)
{
	struct timespec now;
	struct wlr_pointer_button_event event = {.button = button,
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

static void click_at(double x, double y) { click_button_at(x, y, BTN_LEFT); }
static double speaker_x(void)
{
 int count = 2;
 GDir *dir = g_dir_open("/sys/class/power_supply", 0, NULL);
 const char *name;
 while (dir && (name = g_dir_read_name(dir))) {
  char *path = g_build_filename("/sys/class/power_supply", name, "type", NULL), *type = NULL;
  gboolean battery = g_file_get_contents(path, &type, NULL, NULL) && g_str_has_prefix(type, "Battery");
  g_free(path); g_free(type);
  if (battery) { count++; break; }
 }
 if (dir) g_dir_close(dir);
 dir = g_dir_open("/sys/class/backlight", 0, NULL);
 if (dir && g_dir_read_name(dir)) count++;
 if (dir) g_dir_close(dir);
 return selmon->m.width-(19+22*count+58+7)+19+11;
}

static int step(void *data)
{
	if (++attempts >= 70) {
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
		{
			Client *run = find_title("Run");
			struct wlr_box normal = run->geom;
			assert(!client_can_maximize(run));
			assert(!(system_enabled(run) & ((1u<<2)|(1u<<4))));
			xpmaximize(run, 1); assert(!run->maximized);
			normal.width += 200; normal.height += 100;
			resize(run, normal, 0);
			assert(run->geom.width == normal.width-200 && run->geom.height == normal.height-100);
			assert(abs(run->geom.x - (run->mon->w.x+(run->mon->w.width-run->geom.width)/2)) <= 1);
			assert(abs(run->geom.y - (run->mon->w.y+(run->mon->w.height-run->geom.height)/2)) <= 1);
		}
		assert(!popup_count());
		assert(seat->keyboard_state.focused_surface == client_surface(find_title("Run")));
		if (getenv("XPWM_RUN_SCREENSHOT") && !run_snapshot++) {
			spawn(&(Arg)SHCMD("grim \"$XPWM_RUN_SCREENSHOT\""));
			goto again;
		}
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
		if (popup_count() < 2) goto again;
		send_key(KEY_LEFT); phase++; break;
	case 12:
		if (popup_count() > 1) { send_key(KEY_LEFT); goto again; }
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
		if (popup_count() < 2) goto again;
		send_key(KEY_HOME); phase++; break;
	case 21:
		if (popup_count() == 2) { send_key(KEY_RIGHT); goto again; }
		assert(popup_count() == 3);
		send_key(KEY_HOME); phase++; break;
	case 22:
		if (popup_count() != 3) goto again;
		send_key(KEY_END); phase++; break;
	case 23:
		assert(popup_count() == 3);
		send_key(KEY_UP);
		send_key(KEY_DOWN); phase++; break;
	case 24:
		assert(popup_count() == 3);
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
		click_at(40, selmon->m.height-15); phase++; break;
	}
	case 26:
		if (!popup_count()) goto again;
		click_at(280, selmon->m.height-50); phase++; break;
	case 27: {
		LayerSurface *panel = find_layer("xpwm-session-panel");
		if (!panel) goto again;
		check_panel(panel);
		/* A global close binding must not close the app behind a modal layer. */
		assert(keybinding(WLR_MODIFIER_ALT, XKB_KEY_F4));
		if (getenv("XPWM_SHUTDOWN_SCREENSHOT"))
			spawn(&(Arg)SHCMD("grim \"$XPWM_SHUTDOWN_SCREENSHOT\""));
		phase++; break;
	}
	case 28: {
		LayerSurface *panel = find_layer("xpwm-session-panel");
		assert(panel);
		click_at(panel->geom.x+261, panel->geom.y+177); phase++; break;
	}
	case 29:
		if (find_layer("xpwm-session-panel") || find_layer("xpwm-session-shade")) goto again;
		assert(!exclusive_focus);
		assert(seat->keyboard_state.focused_surface == client_surface(find_title("XPWm decoration test")));
		click_at(40, selmon->m.height-15); phase++; break;
	case 30:
		if (!popup_count()) goto again;
		click_at(80, selmon->m.height-50); phase++; break;
	case 31: {
		LayerSurface *panel = find_layer("xpwm-session-panel");
		if (!panel) goto again;
		check_panel(panel);
		if (getenv("XPWM_LOGOFF_DIALOG_SCREENSHOT"))
			spawn(&(Arg)SHCMD("grim \"$XPWM_LOGOFF_DIALOG_SCREENSHOT\""));
		phase++; break;
	}
	case 32:
		send_key(KEY_ESC); phase++; break;
	case 33:
		if (find_layer("xpwm-session-panel") || find_layer("xpwm-session-shade")) goto again;
		assert(!exclusive_focus);
		assert(seat->keyboard_state.focused_surface == client_surface(find_title("XPWm decoration test")));
		click_at(40, selmon->m.height-15); phase++; break;
	case 34:
		if (!popup_count()) goto again;
		click_at(280, selmon->m.height-50); phase++; break;
	case 35: {
		LayerSurface *panel = find_layer("xpwm-session-panel");
		if (!panel) goto again;
		check_panel(panel);
		click_at(panel->geom.x+247, panel->geom.y+92); phase++; break;
	}
	case 36: {
		char action[32] = {0};
		FILE *file;
		if (find_layer("xpwm-session-panel") || find_layer("xpwm-session-shade")) goto again;
		file = fopen(getenv("XPWM_SESSION_MARKER"), "r");
		assert(file); assert(fgets(action, sizeof(action), file)); fclose(file);
		assert(!strcmp(action, "reboot"));
		assert(!exclusive_focus);
		assert(seat->keyboard_state.focused_surface == client_surface(find_title("XPWm decoration test")));
		fprintf(stderr, "PASS: centered XP session panels, modal shade, Cancel/Escape, focus restoration, dry-run Restart click\n");
		click_at(40, selmon->m.height-15); phase++; break;
	}
	case 37:
		if (!popup_count()) goto again;
		send_key(KEY_HOME); phase++; break;
	case 38:
		send_key(KEY_ENTER); phase++; break;
	case 39: {
		char path[1024], name[32] = {0};
		FILE *file;
		snprintf(path, sizeof(path), "%s/launched", getenv("XDG_RUNTIME_DIR"));
		file = fopen(path, "r"); assert(file);
		assert(fgets(name, sizeof(name), file)); fclose(file);
		if (strcmp(name, "Alpha")) goto again;
		assert(!popup_count());
		fprintf(stderr, "PASS: pinned default Internet launcher uses the private Alpha application\n");
		assert(keybinding(MODKEY, XKB_KEY_d));
		phase++; break;
	}
	case 40:
		if (!find_title("Run")) goto again;
		assert(seat->keyboard_state.focused_surface == client_surface(find_title("Run")));
		send_key(KEY_ESC); phase++; break;
	case 41:
		if (find_title("Run")) goto again;
		fprintf(stderr, "PASS: launcher shortcut opens the native XP Run dialog\n");
		assert(keybinding(WLR_MODIFIER_ALT, XKB_KEY_F4)); phase++; break;
	case 42:
		if (find_title("XPWm decoration test")) goto again;
		fprintf(stderr, "PASS: Alt+F4 closes the focused window and preserves apps behind modal layers\n");
		click_at(selmon->m.width-25, selmon->m.height-15);
		phase++; break;
	case 43:
		if (!find_title("Date and Time")) goto again;
		assert(seat->keyboard_state.focused_surface == client_surface(find_title("Date and Time")));
		if (getenv("XPWM_TRAY_BAR_SCREENSHOT"))
			spawn(&(Arg)SHCMD("grim \"$XPWM_TRAY_BAR_SCREENSHOT\""));
		send_key(KEY_ESC); phase++; break;
	case 44:
		if (find_title("Date and Time")) goto again;
		fprintf(stderr, "PASS: native tray clock click opens calendar; Escape dismisses it\n");
  click_at(speaker_x(), selmon->m.height-15); phase++; break;
 case 45: {
  LayerSurface *popup = find_layer("xpwm-volume-popup");
  if (!popup) goto again;
  assert(seat->keyboard_state.focused_surface == popup->layer_surface->surface);
  if (getenv("XPWM_VOLUME_POPUP_SCREENSHOT") && !volume_snapshot++) {
   spawn(&(Arg)SHCMD("grim \"$XPWM_VOLUME_POPUP_SCREENSHOT\""));
   goto again;
  }
  send_key(KEY_ESC); phase++; break;
 }
 case 46:
  if (find_layer("xpwm-volume-popup")) goto again;
  assert(!exclusive_focus);
  click_at(speaker_x(), selmon->m.height-15); phase++; break;
 case 47:
  if (!find_layer("xpwm-volume-popup")) goto again;
  click_at(selmon->m.width/2, selmon->m.height/2); phase++; break;
 case 48:
  if (find_layer("xpwm-volume-popup")) goto again;
  assert(!exclusive_focus);
  click_button_at(speaker_x(), selmon->m.height-15, BTN_RIGHT); phase++; break;
 case 49:
  if (!popup_count()) goto again;
  send_key(KEY_ESC); phase++; break;
 case 50:
  if (popup_count()) goto again;
  assert(!exclusive_focus);
  fprintf(stderr, "PASS: XP volume popup opens from tray, closes on Escape/outside click; tray context menu dismisses cleanly\n");
  click_at(40, selmon->m.height-15); phase++; break;
 case 51:
  if (!popup_count()) goto again;
  click_at(260, selmon->m.height-30-40-30-18); phase++; break;
 case 52:
  if (!find_title("Search Results")) goto again;
  assert(!find_title("Run") && !popup_count());
  assert(seat->keyboard_state.focused_surface == client_surface(find_title("Search Results")));
  send_key(KEY_ESC); phase++; break;
 case 53:
  if (find_title("Search Results")) goto again;
  assert(!exclusive_focus);
  fprintf(stderr, "PASS: Start Search opens its native results window and Escape restores focus\n");
  click_button_at(selmon->m.width/2., selmon->m.height-15, BTN_RIGHT); phase++; break;
 case 54:
  if (!popup_count()) goto again;
  send_key(KEY_END); phase++; break;
 case 55:
  send_key(KEY_ENTER); phase++; break;
 case 56:
  if (!find_title("Taskbar Properties")) goto again;
  assert(seat->keyboard_state.focused_surface == client_surface(find_title("Taskbar Properties")));
  if (getenv("XPWM_TASKBAR_PROPERTIES_SCREENSHOT") && !properties_snapshot++) {
   spawn(&(Arg)SHCMD("grim \"$XPWM_TASKBAR_PROPERTIES_SCREENSHOT\"")); goto again;
  }
  send_key(KEY_ESC); phase++; break;
 case 57:
  if (find_title("Taskbar Properties")) goto again;
  assert(!exclusive_focus && !popup_count());
  fprintf(stderr, "PASS: taskbar right-click Properties opens a native settings window and Escape cancels cleanly\n");
  phase++; wl_display_terminate(dpy); return 0;
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
	/* Native pointer tests must never perform a real session action. */
	assert(getenv("XPWM_SESSION_MARKER"));
	setenv("XPWM_SESSION_DRY_RUN", "1", 1);
	setenv("XPWM_SOUND_MUTE", "1", 1);
	setup();
	wlr_seat_set_capabilities(seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
	timer = wl_event_loop_add_timer(event_loop, step, NULL);
	wl_event_source_timer_update(timer, 300);
	run("sh tests/window-client.sh");
	assert(phase == 58);
	wl_event_source_remove(timer);
	cleanup();
	return 0;
}
