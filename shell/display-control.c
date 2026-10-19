/* Output-management client and independent timed rollback guard. */
#define _POSIX_C_SOURCE 200809L
#include "output-client.h"
#include <glib.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>

typedef struct {
	struct zwlr_output_mode_v1 *proxy;
	int width, height, refresh;
	gboolean live, preferred;
} Mode;
typedef struct {
	struct zwlr_output_head_v1 *proxy;
	char *name, *description;
	GPtrArray *modes;
	Mode *current;
	gboolean live, enabled;
	int x, y, transform;
	double scale;
} Head;
static struct wl_display *display;
static struct zwlr_output_manager_v1 *manager;
static GPtrArray *heads;
static guint serial;
static gboolean ready;

static void mode_size(void *data, struct zwlr_output_mode_v1 *p, int32_t w, int32_t h)
{
	Mode *m = data;
	m->width = w;
	m->height = h;
}
static void mode_refresh(void *data, struct zwlr_output_mode_v1 *p, int32_t r)
{
	((Mode *)data)->refresh = r;
}
static void mode_preferred(void *data, struct zwlr_output_mode_v1 *p) { ((Mode *)data)->preferred = TRUE; }
static void mode_finished(void *data, struct zwlr_output_mode_v1 *p) { ((Mode *)data)->live = FALSE; }
static const struct zwlr_output_mode_v1_listener mode_listener = {mode_size, mode_refresh, mode_preferred,
								  mode_finished};
static void head_name(void *data, struct zwlr_output_head_v1 *p, const char *s)
{
	Head *h = data;
	g_free(h->name);
	h->name = g_strdup(s);
}
static void head_description(void *data, struct zwlr_output_head_v1 *p, const char *s)
{
	Head *h = data;
	g_free(h->description);
	h->description = g_strdup(s);
}
static void head_size(void *data, struct zwlr_output_head_v1 *p, int32_t w, int32_t h) {}
static void head_mode(void *data, struct zwlr_output_head_v1 *p, struct zwlr_output_mode_v1 *proxy)
{
	Mode *m = g_new0(Mode, 1);
	m->proxy = proxy;
	m->live = TRUE;
	g_ptr_array_add(((Head *)data)->modes, m);
	zwlr_output_mode_v1_add_listener(proxy, &mode_listener, m);
}
static void head_enabled(void *data, struct zwlr_output_head_v1 *p, int32_t value)
{
	((Head *)data)->enabled = value;
}
static void head_current(void *data, struct zwlr_output_head_v1 *p, struct zwlr_output_mode_v1 *proxy)
{
	Head *h = data;
	h->current = NULL;
	for (guint i = 0; i < h->modes->len; i++) {
		Mode *m = g_ptr_array_index(h->modes, i);
		if (m->proxy == proxy)
			h->current = m;
	}
}
static void head_position(void *data, struct zwlr_output_head_v1 *p, int32_t x, int32_t y)
{
	Head *h = data;
	h->x = x;
	h->y = y;
}
static void head_transform(void *data, struct zwlr_output_head_v1 *p, int32_t value)
{
	((Head *)data)->transform = value;
}
static void head_scale(void *data, struct zwlr_output_head_v1 *p, wl_fixed_t value)
{
	((Head *)data)->scale = wl_fixed_to_double(value);
}
static void head_finished(void *data, struct zwlr_output_head_v1 *p) { ((Head *)data)->live = FALSE; }
static void head_identity(void *data, struct zwlr_output_head_v1 *p, const char *s) {}
static const struct zwlr_output_head_v1_listener head_listener = {.name = head_name,
								  .description = head_description,
								  .physical_size = head_size,
								  .mode = head_mode,
								  .enabled = head_enabled,
								  .current_mode = head_current,
								  .position = head_position,
								  .transform = head_transform,
								  .scale = head_scale,
								  .finished = head_finished,
								  .make = head_identity,
								  .model = head_identity,
								  .serial_number = head_identity};
static void manager_head(void *data, struct zwlr_output_manager_v1 *p, struct zwlr_output_head_v1 *proxy)
{
	Head *h = g_new0(Head, 1);
	h->proxy = proxy;
	h->live = TRUE;
	h->scale = 1;
	h->modes = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(heads, h);
	zwlr_output_head_v1_add_listener(proxy, &head_listener, h);
}
static void manager_done(void *data, struct zwlr_output_manager_v1 *p, uint32_t value)
{
	serial = value;
	ready = TRUE;
}
static void manager_finished(void *data, struct zwlr_output_manager_v1 *p) { manager = NULL; }
static const struct zwlr_output_manager_v1_listener manager_listener = {manager_head, manager_done,
									manager_finished};
static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface,
		   uint32_t version)
{
	if (!strcmp(interface, zwlr_output_manager_v1_interface.name)) {
		manager =
		    wl_registry_bind(registry, name, &zwlr_output_manager_v1_interface, MIN(version, 2));
		zwlr_output_manager_v1_add_listener(manager, &manager_listener, NULL);
	}
}
static void removed(void *data, struct wl_registry *registry, uint32_t name) {}
static const struct wl_registry_listener registry_listener = {global, removed};

static Head *find_head(const char *name)
{
	for (guint i = 0; i < heads->len; i++) {
		Head *h = g_ptr_array_index(heads, i);
		if (h->live && !g_strcmp0(h->name, name))
			return h;
	}
	return NULL;
}
static Mode *find_mode(Head *h, int w, int height, int refresh)
{
	for (guint i = 0; i < h->modes->len; i++) {
		Mode *m = g_ptr_array_index(h->modes, i);
		if (m->live && m->width == w && m->height == height && m->refresh == refresh)
			return m;
	}
	return NULL;
}

/* Polling with prepare_read keeps deadlines effective even if the compositor
 * stops replying. No desktop process or GUI timer is needed for rollback. */
static gboolean dispatch_until(gint64 deadline)
{
	if (wl_display_dispatch_pending(display) < 0)
		return FALSE;
	while (wl_display_prepare_read(display) != 0)
		if (wl_display_dispatch_pending(display) < 0)
			return FALSE;
	wl_display_flush(display);
	struct pollfd fd = {wl_display_get_fd(display), POLLIN, 0};
	int wait = MAX(0, (int)((deadline - g_get_monotonic_time()) / 1000));
	int result = poll(&fd, 1, wait);
	if (result <= 0) {
		wl_display_cancel_read(display);
		return FALSE;
	}
	if (wl_display_read_events(display) < 0)
		return FALSE;
	return wl_display_dispatch_pending(display) >= 0;
}
static void synchronized(void *data, struct wl_callback *callback, uint32_t serial)
{
	*(gboolean *)data = TRUE;
}
static const struct wl_callback_listener sync_listener = {synchronized};
static gboolean synchronize(void)
{
	gboolean done = FALSE;
	struct wl_callback *callback = wl_display_sync(display);
	wl_callback_add_listener(callback, &sync_listener, &done);
	gint64 deadline = g_get_monotonic_time() + 5000000;
	while (!done && g_get_monotonic_time() < deadline)
		if (!dispatch_until(deadline))
			break;
	wl_callback_destroy(callback);
	return done;
}
static void succeeded(void *data, struct zwlr_output_configuration_v1 *p) { *(int *)data = 1; }
static void failed(void *data, struct zwlr_output_configuration_v1 *p) { *(int *)data = -1; }
static void cancelled(void *data, struct zwlr_output_configuration_v1 *p) { *(int *)data = -2; }
static const struct zwlr_output_configuration_v1_listener config_listener = {succeeded, failed, cancelled};

static gboolean configure(const char *name, int width, int height, int refresh, double scale, gboolean test,
			  gboolean restore)
{
	if (!synchronize())
		return FALSE;
	Head *target = find_head(name);
	if (!manager || !target || !target->enabled)
		return FALSE;
	Mode *wanted = find_mode(target, width, height, refresh);
	if (!wanted && !restore)
		return FALSE;
	int result = 0;
	struct zwlr_output_configuration_v1 *config =
	    zwlr_output_manager_v1_create_configuration(manager, serial);
	zwlr_output_configuration_v1_add_listener(config, &config_listener, &result);
	for (guint i = 0; i < heads->len; i++) {
		Head *h = g_ptr_array_index(heads, i);
		if (!h->live)
			continue;
		if (!h->enabled) {
			zwlr_output_configuration_v1_disable_head(config, h->proxy);
			continue;
		}
		struct zwlr_output_configuration_head_v1 *c =
		    zwlr_output_configuration_v1_enable_head(config, h->proxy);
		Mode *m = h == target ? wanted : h->current;
		if (m && m->live)
			zwlr_output_configuration_head_v1_set_mode(c, m->proxy);
		else if (h == target && restore)
			zwlr_output_configuration_head_v1_set_custom_mode(c, width, height, refresh);
		else {
			zwlr_output_configuration_v1_destroy(config);
			return FALSE;
		}
		zwlr_output_configuration_head_v1_set_position(c, h->x, h->y);
		zwlr_output_configuration_head_v1_set_transform(c, h->transform);
		zwlr_output_configuration_head_v1_set_scale(
		    c, wl_fixed_from_double(h == target ? scale : h->scale));
	}
	if (test)
		zwlr_output_configuration_v1_test(config);
	else
		zwlr_output_configuration_v1_apply(config);
	gint64 deadline = g_get_monotonic_time() + 5000000;
	while (!result && g_get_monotonic_time() < deadline)
		if (!dispatch_until(deadline))
			break;
	zwlr_output_configuration_v1_destroy(config);
	wl_display_dispatch_pending(display);
	return result == 1;
}

static void list_heads(void)
{
	GKeyFile *ini = g_key_file_new();
	int count = 0;
	for (guint i = 0; i < heads->len; i++) {
		Head *h = g_ptr_array_index(heads, i);
		if (!h->live)
			continue;
		char group[64];
		g_snprintf(group, sizeof(group), "display%d", count++);
		g_key_file_set_string(ini, group, "name", h->name ? h->name : "");
		g_key_file_set_string(ini, group, "description", h->description ? h->description : "");
		g_key_file_set_boolean(ini, group, "enabled", h->enabled);
		g_key_file_set_double(ini, group, "scale", h->scale);
		g_key_file_set_integer(ini, group, "x", h->x);
		g_key_file_set_integer(ini, group, "y", h->y);
		g_key_file_set_integer(ini, group, "width", h->current ? h->current->width : 0);
		g_key_file_set_integer(ini, group, "height", h->current ? h->current->height : 0);
		g_key_file_set_integer(ini, group, "refresh", h->current ? h->current->refresh : 0);
		int modes = 0;
		for (guint j = 0; j < h->modes->len; j++) {
			Mode *m = g_ptr_array_index(h->modes, j);
			if (!m->live)
				continue;
			char key[64], value[100];
			g_snprintf(key, sizeof(key), "mode%d", modes++);
			g_snprintf(value, sizeof(value), "%d %d %d", m->width, m->height, m->refresh);
			g_key_file_set_string(ini, group, key, value);
		}
		g_key_file_set_integer(ini, group, "modes", modes);
	}
	g_key_file_set_integer(ini, "outputs", "count", count);
	char *text = g_key_file_to_data(ini, NULL, NULL);
	fputs(text, stdout);
	g_free(text);
	g_key_file_unref(ini);
}

static gboolean save_settings(const char *name, int width, int height, int refresh, double scale)
{
	char *dir = g_build_filename(g_get_user_config_dir(), "xpwm", NULL);
	char *path = g_build_filename(dir, "displays.ini", NULL);
	gboolean ok = g_mkdir_with_parents(dir, 0700) == 0;
	GKeyFile *ini = g_key_file_new();
	g_key_file_load_from_file(ini, path, G_KEY_FILE_NONE, NULL);
	char *group = g_strdup_printf("display:%s", name);
	Head *h = find_head(name);
	g_key_file_set_string(ini, group, "description", h && h->description ? h->description : "");
	g_key_file_set_integer(ini, group, "width", width);
	g_key_file_set_integer(ini, group, "height", height);
	g_key_file_set_integer(ini, group, "refresh", refresh);
	g_key_file_set_double(ini, group, "scale", scale);
	char *text = g_key_file_to_data(ini, NULL, NULL);
	ok = ok && g_file_set_contents(path, text, -1, NULL);
	g_free(text);
	g_free(group);
	g_key_file_unref(ini);
	g_free(path);
	g_free(dir);
	return ok;
}
static void restore_saved(void)
{
	char *path = g_build_filename(g_get_user_config_dir(), "xpwm", "displays.ini", NULL);
	GKeyFile *ini = g_key_file_new();
	if (g_key_file_load_from_file(ini, path, G_KEY_FILE_NONE, NULL))
		for (guint i = 0; i < heads->len; i++) {
			Head *h = g_ptr_array_index(heads, i);
			if (!h->live || !h->enabled || !h->name || !h->current)
				continue;
			char *group = g_strdup_printf("display:%s", h->name);
			char *description = g_key_file_get_string(ini, group, "description", NULL);
			int w = g_key_file_get_integer(ini, group, "width", NULL),
			    height = g_key_file_get_integer(ini, group, "height", NULL),
			    r = g_key_file_get_integer(ini, group, "refresh", NULL);
			double scale = g_key_file_get_double(ini, group, "scale", NULL);
			int old_w = h->current->width, old_h = h->current->height,
			    old_r = h->current->refresh;
			double old_scale = h->scale;
			if (!g_strcmp0(description, h->description) && isfinite(scale) && scale >= .5 &&
			    scale <= 4 && find_mode(h, w, height, r)) {
				if (configure(h->name, w, height, r, scale, TRUE, FALSE) &&
				    !configure(h->name, w, height, r, scale, FALSE, FALSE))
					configure(h->name, old_w, old_h, old_r, old_scale, FALSE, TRUE);
			}
			g_free(description);
			g_free(group);
		}
	g_key_file_unref(ini);
	g_free(path);
}

int main(int argc, char **argv)
{
	signal(SIGPIPE, SIG_IGN);
	setvbuf(stdout, NULL, _IOLBF, 0);
	heads = g_ptr_array_new();
	display = wl_display_connect(NULL);
	if (!display) {
		puts("ERROR Cannot connect to the desktop.");
		return 1;
	}
	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	if (wl_display_roundtrip(display) < 0 || !manager) {
		puts("ERROR Display configuration is unavailable.");
		return 1;
	}
	gint64 deadline = g_get_monotonic_time() + 5000000;
	while (!ready && g_get_monotonic_time() < deadline)
		if (!dispatch_until(deadline))
			break;
	if (!ready) {
		puts("ERROR Displays did not respond.");
		return 1;
	}
	if (argc == 2 && !strcmp(argv[1], "--restore")) {
		restore_saved();
		return 0;
	}
	if (argc == 2 && !strcmp(argv[1], "--list")) {
		list_heads();
		return 0;
	}
	if (argc != 7 || strcmp(argv[1], "--preview")) {
		puts("ERROR Invalid display request.");
		return 1;
	}
	char *end = NULL;
	double scale = g_ascii_strtod(argv[6], &end);
	Head *h = find_head(argv[2]);
	int w = atoi(argv[3]), height = atoi(argv[4]), refresh = atoi(argv[5]);
	if (!end || *end || !isfinite(scale) || scale < .5 || scale > 4 || !h || !h->enabled || !h->current ||
	    !find_mode(h, w, height, refresh)) {
		puts("ERROR Choose a supported display mode and scale.");
		return 1;
	}
	int old_w = h->current->width, old_h = h->current->height, old_refresh = h->current->refresh;
	double old_scale = h->scale;
	if (!configure(argv[2], w, height, refresh, scale, TRUE, FALSE)) {
		puts("ERROR This display configuration is not supported.");
		return 1;
	}
	if (!configure(argv[2], w, height, refresh, scale, FALSE, FALSE)) {
		configure(argv[2], old_w, old_h, old_refresh, old_scale, FALSE, TRUE);
		puts("ERROR Unable to apply display settings.");
		return 1;
	}
	puts("APPLIED");
	deadline = g_get_monotonic_time() + 15000000;
	char input[64] = {0};
	size_t length = 0;
	while (g_get_monotonic_time() < deadline) {
		struct pollfd fds[] = {{STDIN_FILENO, POLLIN, 0}, {wl_display_get_fd(display), POLLIN, 0}};
		wl_display_flush(display);
		int wait = MAX(0, (int)((deadline - g_get_monotonic_time()) / 1000));
		int result = poll(fds, 2, wait);
		if (result <= 0)
			break;
		if (fds[0].revents) {
			ssize_t n = read(STDIN_FILENO, input + length, sizeof(input) - 1 - length);
			if (n <= 0)
				break;
			length += n;
			input[length] = 0;
			if (strchr(input, '\n')) {
				if (!strcmp(input, "keep\n")) {
					gboolean saved = save_settings(argv[2], w, height, refresh, scale);
					puts("KEPT");
					if (!saved)
						puts("UNSAVED");
					return 0;
				}
				break;
			}
			if (length == sizeof(input) - 1)
				break;
		}
		if (fds[1].revents && wl_display_dispatch(display) < 0)
			break;
	}
	if (configure(argv[2], old_w, old_h, old_refresh, old_scale, FALSE, TRUE)) {
		puts("REVERTED");
		return 0;
	}
	puts("ERROR Unable to restore the previous display settings. Reopen Display Properties.");
	return 1;
}
