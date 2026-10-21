#define main dwl_original_main
#include "../compositor/dwl.c"
#undef main
#include <assert.h>

static struct wl_event_source *test_timer;
static int attempts, passed;

static void check_monitor(Monitor *m, const char *expected, const char *requested)
{
	assert(!strcmp(m->lt[m->sellt]->symbol, expected));
	if (!requested) return;
	selmon = m;
	xpallfloat(&(Arg){0});
	assert(!strcmp(m->lt[m->sellt]->symbol, requested));
}

static int step(void *data)
{
	Monitor *m, *first = NULL, *second = NULL;
	(void)data;
	assert(++attempts < 30);
	wl_list_for_each(m, &mons, link) {
		if (!strcmp(m->wlr_output->name, "HEADLESS-1")) first = m;
		if (!strcmp(m->wlr_output->name, "HEADLESS-2")) second = m;
	}
	if (!first || !second) { wl_event_source_timer_update(test_timer, 50); return 0; }
	check_monitor(first, getenv("XPWM_EXPECT_FIRST"), getenv("XPWM_SAVE_FIRST"));
	check_monitor(second, getenv("XPWM_EXPECT_SECOND"), getenv("XPWM_SAVE_SECOND"));
	passed = 1;
	fprintf(stderr, "PASS: output modes restored and saved independently across compositor sessions\n");
	wl_display_terminate(dpy); return 0;
}

int main(void)
{
	setup();
	test_timer = wl_event_loop_add_timer(event_loop, step, NULL);
	wl_event_source_timer_update(test_timer, 50);
	run(NULL); assert(passed);
	wl_event_source_remove(test_timer); cleanup(); return 0;
}
