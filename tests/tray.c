/* Native controls backed by private fake services; never changes host devices. */
#include "../shell/tray.c"
#include <glib/gstdio.h>
#include "../shared/luna.h"

/* Compare actual rendered tray icon pixels against the decoded originals,
 * including transparency, so a scaled or hand-drawn replacement fails. */
static void verify_original_pixels(void)
{
 const struct { gboolean mute, connected; const char *sound, *network; guint activity; } cases[] = {
  {FALSE, TRUE, "volume", "network-idle", 0},
  {TRUE, TRUE, "volume-muted", "network-idle", 0},
  {FALSE, FALSE, "volume", "network-disconnected", 3},
  {FALSE, TRUE, "volume", "network-receive", 1},
  {FALSE, TRUE, "volume", "network-transmit", 2},
  {FALSE, TRUE, "volume", "network-connected", 3},
 };
 char *saved_network = network_text;
 gboolean saved_mute = muted;
 guint saved_activity = network_activity; network_activity = 0;
 for (guint i = 0; i < G_N_ELEMENTS(cases); i++) {
  muted = cases[i].mute; network_activity = cases[i].activity;
  network_text = cases[i].connected ? "Connected\nInternet access" : "Disconnected";
  cairo_surface_t *actual = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, xp_tray_width(), 30);
  cairo_surface_t *expected = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, xp_tray_width(), 30);
  cairo_t *a = cairo_create(actual), *e = cairo_create(expected);
  xp_tray_draw(a, 0, 30);
  for (int slot = 0; slot < 2; slot++) {
   char *path = g_strdup_printf("xp-tray/%s.png", slot ? cases[i].network : cases[i].sound);
   g_assert_true(xp_bitmap(e, path, TRAY_LEFT+3+slot*SLOT, 7));
   g_free(path);
  }
  cairo_surface_flush(actual); cairo_surface_flush(expected);
  int stride = cairo_image_surface_get_stride(actual);
  for (int y = 0; y < 30; y++)
   g_assert_cmpmem(cairo_image_surface_get_data(actual)+y*stride, (TRAY_LEFT+2*SLOT)*4,
    cairo_image_surface_get_data(expected)+y*stride, (TRAY_LEFT+2*SLOT)*4);
  cairo_destroy(a); cairo_destroy(e); cairo_surface_destroy(actual); cairo_surface_destroy(expected);
 }
 network_text = saved_network; muted = saved_mute; network_activity = saved_activity;
 g_assert_cmpint(xp_tray_hit(TRAY_LEFT-1), ==, -1);
 g_assert_cmpint(xp_tray_hit(TRAY_LEFT), ==, 0);
 g_assert_cmpint(xp_tray_hit(TRAY_LEFT+SLOT), ==, 1);
 g_assert_cmpint(xp_tray_hit(xp_tray_width()-1), ==, 3);
 g_print("PASS: tray icon pixels match original XP resources at native size; theme padding and hit regions agree\n");
}


static void verify_traffic(void)
{
 char *root = g_dir_make_tmp("xpwm-net-test-XXXXXX", NULL);
 char *stats = g_build_filename(root, "eth0", "statistics", NULL);
 g_assert_cmpint(g_mkdir_with_parents(stats, 0700), ==, 0);
 char *state = g_build_filename(root, "eth0", "operstate", NULL);
 char *rx = g_build_filename(stats, "rx_bytes", NULL), *tx = g_build_filename(stats, "tx_bytes", NULL);
 g_file_set_contents(state, "up\n", -1, NULL);
 g_file_set_contents(rx, "100", -1, NULL); g_file_set_contents(tx, "100", -1, NULL);
 read_traffic(root); g_assert_cmpuint(network_activity, ==, 0);
 g_file_set_contents(rx, "101", -1, NULL); read_traffic(root); g_assert_cmpuint(network_activity, ==, 1);
 g_file_set_contents(tx, "102", -1, NULL); read_traffic(root); g_assert_cmpuint(network_activity, ==, 2);
 g_file_set_contents(rx, "103", -1, NULL); g_file_set_contents(tx, "103", -1, NULL);
 read_traffic(root); g_assert_cmpuint(network_activity, ==, 3);
 char *saved = network_text; network_text = "Connected\nLimited connectivity";
 g_assert_cmpstr(network_icon(), ==, "network-connected");
 read_traffic(root); g_assert_cmpstr(network_icon(), ==, "network-idle");
 network_text = "Disconnected"; g_assert_cmpstr(network_icon(), ==, "network-disconnected");
 network_text = saved;
 g_file_set_contents(rx, "1", -1, NULL); g_file_set_contents(tx, "1", -1, NULL);
 read_traffic(root); g_assert_cmpuint(network_activity, ==, 0);
 g_remove(rx); g_remove(tx); g_remove(state); g_rmdir(stats);
 char *iface = g_path_get_dirname(stats); g_rmdir(iface); g_rmdir(root);
 g_free(root); g_free(stats); g_free(state); g_free(rx); g_free(tx); g_free(iface);
 g_clear_pointer(&traffic, g_hash_table_unref);
 g_print("PASS: XP network activity icons reflect receive/transmit/both/idle; LAN without Internet stays connected\n");
}

static int stage, attempts;

static char *fixture(const char *name)
{
	return g_build_filename(g_getenv("XDG_RUNTIME_DIR"), name, NULL);
}

static void write_fixture(const char *name, const char *value)
{
	char *path = fixture(name); g_assert_true(g_file_set_contents(path, value, -1, NULL)); g_free(path);
}

static gboolean state_is(const char *name, const char *expected)
{
	char *path = fixture(name), *value = NULL;
	g_file_get_contents(path, &value, NULL, NULL);
	gboolean matches = !g_strcmp0(value, expected); g_free(path); g_free(value); return matches;
}

static gboolean check(gpointer data)
{
	g_assert_cmpint(++attempts, <, 100);
	switch (stage) {
	case 0:
		if (!audio_ok || !network_ok) break;
		g_assert_cmpint(volume, ==, 45); g_assert_false(muted);
		verify_original_pixels(); verify_traffic();
  xp_tray_click(TRAY_LEFT+5, 1); g_assert_cmpuint(volume_click_timer, >, 0);
  xp_tray_double_click(TRAY_LEFT+5); g_assert_cmpuint(volume_click_timer, ==, 0);
  g_assert_null(volume_window);
		g_assert_nonnull(strstr(network_text, "Internet access"));
		g_clear_pointer(&battery_path, g_free); battery_path = fixture("battery"); refresh();
		g_assert_cmpint(capacity, ==, 74);
		open_volume(); g_assert_nonnull(volume_window);
		g_assert_true(gtk_layer_is_layer_window(GTK_WINDOW(volume_window)));
		g_assert_cmpint(gtk_orientable_get_orientation(GTK_ORIENTABLE(volume_scale)), ==, GTK_ORIENTATION_VERTICAL);
  g_assert_cmpint(gtk_widget_get_allocated_width(volume_panel), <=, 90);
		open_volume(); g_assert_null(volume_window); open_volume();
		gtk_range_set_value(GTK_RANGE(volume_scale), 72);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mute_check), TRUE);
		stage++; break;
	case 1:
		if (audio_busy || !state_is("volume", "72") || !state_is("mute", "1")) break;
		if (g_getenv("XPWM_TRAY_SCREENSHOT")) {
			char *quoted = g_shell_quote(g_getenv("XPWM_TRAY_SCREENSHOT"));
			char *command = g_strdup_printf("grim %s", quoted);
			g_assert_true(g_spawn_command_line_sync(command, NULL, NULL, NULL, NULL));
			g_free(command); g_free(quoted);
		}
		gtk_range_set_value(GTK_RANGE(volume_scale), 80);
		gtk_range_set_value(GTK_RANGE(volume_scale), 20);
		gtk_range_set_value(GTK_RANGE(volume_scale), 62);
		xp_tray_scroll(TRAY_LEFT+5, GDK_SCROLL_UP);
  xp_tray_click(TRAY_LEFT+5, 3); g_assert_nonnull(tray_menu);
  GList *entries = gtk_container_get_children(GTK_CONTAINER(tray_menu));
  g_assert_cmpstr(gtk_menu_item_get_label(GTK_MENU_ITEM(entries->data)), ==, "Open Volume Control");
  g_list_free(entries); tray_menu_closed(tray_menu, NULL);
  open_volume(); gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mute_check), FALSE);
		stage++; break;
	case 2:
		if (audio_busy || !state_is("volume", "67") || !state_is("mute", "0")) break;
		xp_tray_click(TRAY_LEFT+SLOT+5, 1); g_assert_nonnull(network_window);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wifi_check), FALSE);
		stage++; break;
	case 3:
		if (network_busy || !state_is("wifi", "disabled")) break;
		write_fixture("connectivity", "portal"); refresh(); stage++; break;
	case 4:
		if (!strstr(network_text, "Sign-in required")) break;
		xp_tray_click(xp_tray_width()-10, 1); g_assert_nonnull(date_window);
		xp_tray_click(TRAY_LEFT+2*SLOT+5, 1); g_assert_nonnull(power_window);
		g_assert_nonnull(strstr(gtk_label_get_text(GTK_LABEL(power_label)), "74%"));
		write_fixture("audio-fail", "1"); write_fixture("network-fail", "1"); refresh(); stage++; break;
	case 5:
		if (audio_ok || network_ok || audio_query || network_query) break;
		g_assert_true(state_is("advanced-opened", "1"));
		g_assert_false(gtk_widget_get_sensitive(volume_scale));
		g_assert_false(gtk_widget_get_sensitive(mute_check));
		g_assert_false(gtk_widget_get_sensitive(wifi_check));
		GdkEventKey escape = {.keyval = GDK_KEY_Escape};
  g_assert_true(popup_key(volume_window, &escape, NULL));
  g_assert_null(volume_window);
  open_volume();
  GdkEventButton outside = {.button = 1, .x = 1, .y = 1};
  g_assert_true(popup_outside(volume_window, &outside, NULL));
		gtk_dialog_response(GTK_DIALOG(network_window), GTK_RESPONSE_CLOSE);
		gtk_dialog_response(GTK_DIALOG(date_window), GTK_RESPONSE_CLOSE);
		gtk_dialog_response(GTK_DIALOG(power_window), GTK_RESPONSE_CLOSE);
		g_assert_null(volume_window); g_assert_null(network_window);
		g_assert_null(date_window); g_assert_null(power_window);
		g_print("PASS: tray vertical popup, Escape/outside dismissal, context menu, coalesced volume/mute, Wi-Fi, calendar, battery and unavailable services\n");
		write_fixture("passed", "passed");
		gtk_main_quit(); return G_SOURCE_REMOVE;
	}
	return G_SOURCE_CONTINUE;
}

int main(int argc, char **argv)
{
	luna_init(); gtk_init(&argc, &argv);
	GtkWidget *canvas = gtk_drawing_area_new();
	xp_tray_init(canvas); g_timeout_add(100, check, NULL);
	gtk_main(); xp_tray_stop(); gtk_widget_destroy(canvas); luna_finish(); return 0;
}
