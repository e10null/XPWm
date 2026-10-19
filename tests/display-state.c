/* Headless outputs advertising multiple physical modes and fractional rates. */
#include <wlr/types/wlr_output.h>
/* The headless backend only implements custom timings. Translate our
 * advertised hardware-mode fixtures at its boundary, keeping the complete
 * output-management protocol, state validation and real commits intact. */
static void headless_mode(struct wlr_output_state *copy,const struct wlr_output_state *state)
{
 *copy=*state;
 if((state->committed&WLR_OUTPUT_STATE_MODE)&&state->mode_type==WLR_OUTPUT_STATE_MODE_FIXED&&state->mode) {
  copy->mode_type=WLR_OUTPUT_STATE_MODE_CUSTOM;
  copy->custom_mode.width=state->mode->width;copy->custom_mode.height=state->mode->height;
  copy->custom_mode.refresh=state->mode->refresh;
 }
}
static bool headless_test(struct wlr_output *output,const struct wlr_output_state *state)
{struct wlr_output_state copy;headless_mode(&copy,state);if((copy.committed&WLR_OUTPUT_STATE_MODE)&&copy.custom_mode.width==1024)return false;return wlr_output_test_state(output,&copy);}
static bool headless_commit(struct wlr_output *output,const struct wlr_output_state *state)
{struct wlr_output_state copy;headless_mode(&copy,state);return wlr_output_commit_state(output,&copy);}
#define wlr_output_test_state headless_test
#define wlr_output_commit_state headless_commit
#define main dwl_original_main
#include "../compositor/dwl.c"
#undef main
#undef wlr_output_test_state
#undef wlr_output_commit_state
#include <assert.h>

static struct wl_event_source *test_timer;
static int attempts, launched, passed, dropdown_clicked, dropdown_verified;
static void dropdown_check(void)
{
 Client *c;
 struct wlr_xdg_surface *surface;
 struct wlr_xdg_client *xc;
 char path[4096];
 int x, y, width, height, fields, px, py;
 Client *parent;
 struct wlr_box moved, popup;
 struct wlr_pointer_button_event event;
 FILE *file;
 if (dropdown_verified) return;
 wl_list_for_each(c, &clients, link) {
  if (!client_get_title(c) || strcmp(client_get_title(c), "Display Properties")) continue;
  snprintf(path, sizeof(path), "%s/display-dropdown-anchor", getenv("XDG_RUNTIME_DIR"));
  file = fopen(path, "r"); if (!file) return;
  fields = fscanf(file, "%d %d %d %d", &x, &y, &width, &height); fclose(file);
  if (fields != 4) return;
  if (!dropdown_clicked) {
   moved = c->geom;
   moved.x = c->mon->w.x + c->mon->w.width - moved.width - 5;
   moved.y = c->mon->w.y + 20;
   resize(c, moved, 1);
   event = (struct wlr_pointer_button_event){.button = BTN_LEFT, .state = WL_POINTER_BUTTON_STATE_PRESSED, .time_msec = 1};
   wlr_cursor_warp_closest(cursor, NULL, c->geom.x+c->bw+x+width/2., c->geom.y+c->bw+XP_CAPTION+y+height/2.);
   motionnotify(1, NULL, 0, 0, 0, 0); buttonpress(NULL, &event);
   event.state = WL_POINTER_BUTTON_STATE_RELEASED; buttonpress(NULL, &event);
   wlr_seat_pointer_notify_frame(seat); dropdown_clicked = 1; return;
  }
  wl_list_for_each(xc, &xdg_shell->clients, link) wl_list_for_each(surface, &xc->surfaces, link) {
   if (surface->role != WLR_XDG_SURFACE_ROLE_POPUP || !surface->surface->mapped) continue;
   parent = NULL;
   toplevel_from_wlr_surface(surface->surface, &parent, NULL);
   if (parent != c) continue;
   popup = surface->popup->current.geometry;
   px = c->geom.x + (int)c->bw + popup.x;
   py = c->geom.y + (int)c->bw + XP_CAPTION + popup.y;
   fprintf(stderr, "Display dropdown field=%dx%d popup=%dx%d at %d,%d anchor=%d,%d\n", width, height, popup.width, popup.height, popup.x, popup.y, x, y);
   assert(popup.width >= width && popup.width <= width+12);
   assert(abs(popup.x-x) <= 12);
   assert(abs(popup.y-(y+height)) <= 12);
   assert(px >= c->mon->m.x && px+popup.width <= c->mon->m.x+c->mon->m.width);
   assert(py >= c->mon->m.y && py+popup.height <= c->mon->m.y+c->mon->m.height);
   snprintf(path, sizeof(path), "%s/display-dropdown-checked", getenv("XDG_RUNTIME_DIR"));
   file = fopen(path, "w"); assert(file); fputs("passed", file); fclose(file);
   dropdown_verified = 1;
   if (getenv("XPWM_DISPLAY_POPUP_SCREENSHOT"))
    spawn(&(Arg)SHCMD("grim \"$XPWM_DISPLAY_POPUP_SCREENSHOT\""));
   fprintf(stderr, "PASS: real refresh-rate popup stays anchored below its field and within the output near its edge\n");
   return;
  }
 }
}
static int check(void *data)
{
	char path[4096];
	dropdown_check();
	if(++attempts>400) {fprintf(stderr,"Display configuration test timed out\n");abort();}
	if(!launched && !wl_list_empty(&mons)) {
		Monitor *m;
		wl_list_for_each(m,&mons,link) {
			const int values[][3]={{1280,720,59940},{1280,720,60000},{1600,900,60000},{1024,768,60000}};
			for(size_t i=0;i<LENGTH(values);i++) {
				struct wlr_output_mode *mode=calloc(1,sizeof(*mode));assert(mode);
				mode->width=values[i][0];mode->height=values[i][1];mode->refresh=values[i][2];
				wl_list_insert(m->wlr_output->modes.prev,&mode->link);
			}
		}
		updatemons(NULL,NULL);
		spawn(&(Arg)SHCMD("python3 tests/display-control.py"));launched=1;
	}
	snprintf(path,sizeof(path),"%s/display-passed",getenv("XDG_RUNTIME_DIR"));
	if(access(path,F_OK)==0) {passed=1;wl_display_terminate(dpy);return 0;}
	snprintf(path,sizeof(path),"%s/display-failed",getenv("XDG_RUNTIME_DIR"));
	if(access(path,F_OK)==0) {wl_display_terminate(dpy);return 0;}
	wl_event_source_timer_update(test_timer,100);return 0;
}
int main(void)
{
	setup();wlr_seat_set_capabilities(seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);test_timer=wl_event_loop_add_timer(event_loop,check,NULL);
	wl_event_source_timer_update(test_timer,100);run(NULL);
	assert(passed && dropdown_verified);wl_event_source_remove(test_timer);cleanup();return 0;
}
