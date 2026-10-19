/* Native Settings controls exercised against the private headless compositor. */
#include "../shell/display-settings.c"
#include "../shared/luna.h"
static GtkWidget *test_window, *test_page;
static Settings *state;
static int phase = -2, attempts;
static gboolean check(gpointer data)
{
	g_assert_cmpint(++attempts, <, 100);
	switch (phase) {
	case -2: {
		if (state->query || !selected(state)) break;
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(state->resolution), "1280x720");
		int x, y;
		g_assert_true(gtk_widget_translate_coordinates(state->rate, test_window, 0, 0, &x, &y));
		char *path = g_build_filename(g_getenv("XDG_RUNTIME_DIR"), "display-dropdown-anchor", NULL);
		char *anchor = g_strdup_printf("%d %d %d %d", x, y, gtk_widget_get_allocated_width(state->rate), gtk_widget_get_allocated_height(state->rate));
		g_file_set_contents(path, anchor, -1, NULL); g_free(path); g_free(anchor);
		phase++; break;
	}
	case -1: {
		char *path = g_build_filename(g_getenv("XDG_RUNTIME_DIR"), "display-dropdown-checked", NULL);
		gboolean checked = g_file_test(path, G_FILE_TEST_EXISTS); g_free(path);
		if (!checked) break;
		gtk_combo_box_popdown(GTK_COMBO_BOX(state->rate));
		phase++; break;
	}
	case 0:
		if (state->query || !selected(state))
			break;
		g_assert_cmpuint(state->outputs->len, ==, 2);
		g_assert_true(gtk_combo_box_set_active_id(GTK_COMBO_BOX(state->resolution), "1280x720"));
		g_assert_true(gtk_combo_box_set_active_id(GTK_COMBO_BOX(state->rate), "59940"));
		g_assert_true(gtk_combo_box_set_active_id(GTK_COMBO_BOX(state->scale), "1.25"));
		g_assert_true(xp_display_settings_dirty(test_page));
		xp_display_settings_apply(test_page, NULL, NULL);
		phase++;
		break;
	case 1:
		if (!state->confirm)
			break;
		gtk_dialog_response(GTK_DIALOG(state->confirm), GTK_RESPONSE_YES);
		phase++;
		break;
	case 2:
		if (state->busy || state->query)
			break;
		g_assert_true(state->kept);
		g_assert_cmpfloat_with_epsilon(selected(state)->scale, 1.25, .001);
		g_assert_cmpint(selected(state)->refresh, ==, 59940);
		g_assert_false(xp_display_settings_dirty(test_page));
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(state->scale), "1.5");
		xp_display_settings_apply(test_page, NULL, NULL);
		phase++;
		break;
	case 3:
		if (!state->confirm)
			break;
		gtk_dialog_response(GTK_DIALOG(state->confirm), GTK_RESPONSE_NO);
		phase++;
		break;
	case 4:
		if (state->busy || state->query)
			break;
		g_assert_false(state->kept);
		g_assert_cmpfloat_with_epsilon(selected(state)->scale, 1.25, .001);
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(state->scale), "1.75");
		xp_display_settings_apply(test_page, NULL, NULL);
		phase++;
		break;
	case 5:
		if (!state->confirm)
			break;
		gtk_widget_destroy(test_window);
		phase++;
		break;
	case 6:
		if (state->busy || state->guard)
			break;
		g_assert_true(state->dead);
		g_print("PASS: Settings monitor/mode/rate/scale controls, Keep/Revert, async refresh and "
			"parent-close rollback\n");
		gtk_main_quit();
		return G_SOURCE_REMOVE;
	}
	return G_SOURCE_CONTINUE;
}
int main(int argc, char **argv)
{
	luna_init();
	gtk_init(&argc, &argv);
	test_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(test_window), "Display Properties");
	test_page = xp_display_settings_new(GTK_WINDOW(test_window), NULL, NULL);
	g_object_ref(test_page);
	state = get(test_page);
	gtk_container_add(GTK_CONTAINER(test_window), test_page);
	xp_style_apply(test_window);
	gtk_widget_show_all(test_window);
	g_timeout_add(100, check, NULL);
	gtk_main();
	g_object_unref(test_page);
	luna_finish();
	return 0;
}
