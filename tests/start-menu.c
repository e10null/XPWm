/* Exercise desktop metadata policy without requiring a display server. */
#include "../shell/start-menu.c"

static void recent_fixtures(void)
{
 g_assert_nonnull(g_getenv("XPWM_MENU_STRUCTURE_TEST"));
 const char *root = g_get_user_data_dir();
 GBookmarkFile *bookmarks = g_bookmark_file_new();
 const char *names[] = {"Visible document.txt", "Private document.txt", "Missing document.txt"};
 for (guint i = 0; i < G_N_ELEMENTS(names); i++) {
  char *path = g_build_filename(root, names[i], NULL);
  if (i < 2) g_assert_true(g_file_set_contents(path, "fixture", -1, NULL));
  char *uri = g_filename_to_uri(path, NULL, NULL);
  g_bookmark_file_set_title(bookmarks, uri, names[i]);
  g_bookmark_file_set_mime_type(bookmarks, uri, "text/plain");
  g_bookmark_file_add_application(bookmarks, uri, "XPWm fixture", "/bin/true %u");
  g_bookmark_file_set_is_private(bookmarks, uri, i == 1);
  g_free(uri); g_free(path);
 }
 char *file = g_build_filename(root, "recently-used.xbel", NULL);
 g_assert_true(g_bookmark_file_to_file(bookmarks, file, NULL));
 g_free(file); g_bookmark_file_free(bookmarks);
}
static void check_menu(void)
{
	recent_fixtures();
 GtkWidget *menu = start_menu_new();
 GList *root_items = gtk_container_get_children(GTK_CONTAINER(menu));
 GtkWidget *recent = NULL;
 for (GList *l = root_items; l; l = l->next)
  if (!g_strcmp0(gtk_menu_item_get_label(GTK_MENU_ITEM(l->data)), "My Recent Documents"))
   recent = gtk_menu_item_get_submenu(GTK_MENU_ITEM(l->data));
 g_assert_nonnull(recent);
 GList *documents = gtk_container_get_children(GTK_CONTAINER(recent));
 g_assert_cmpuint(g_list_length(documents), ==, 1);
 g_assert_cmpstr(gtk_menu_item_get_label(GTK_MENU_ITEM(documents->data)), ==, "Visible document.txt");
 g_list_free(documents); g_list_free(root_items);
 g_print("PASS: Recent Documents includes existing local items and omits private/missing entries\n");
	GtkWidget *programs = g_object_get_data(G_OBJECT(menu), "programs");
	GList *folders = gtk_container_get_children(GTK_CONTAINER(programs));
	GHashTable *seen = g_hash_table_new(g_str_hash, g_str_equal);
	const char *previous_folder = NULL;
	for (GList *f = folders; f; f = f->next) {
		const char *name = gtk_menu_item_get_label(GTK_MENU_ITEM(f->data));
		GtkWidget *submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(f->data));
		g_assert_nonnull(submenu);
		if (previous_folder) g_assert_cmpint(g_utf8_collate(previous_folder, name), <, 0);
		previous_folder = name;
		GList *items = gtk_container_get_children(GTK_CONTAINER(submenu));
		const char *previous_app = NULL;
		for (GList *l = items; l; l = l->next) {
			GAppInfo *app = g_object_get_data(G_OBJECT(l->data), "application");
			g_assert_nonnull(app);
			g_assert_true(gtk_widget_get_visible(l->data));
			g_assert_true(g_app_info_should_show(app));
			g_assert_cmpstr(app_folder(app), ==, name);
			g_assert_false(g_hash_table_contains(seen, g_app_info_get_id(app)));
			g_hash_table_add(seen, (gpointer)g_app_info_get_id(app));
			const char *display = g_app_info_get_display_name(app);
			if (previous_app) g_assert_cmpint(g_utf8_collate(previous_app, display), <=, 0);
			previous_app = display;
		}
		g_list_free(items);
	}
	g_assert_cmpuint(g_hash_table_size(seen), ==, 6);
	g_assert_cmpuint(g_list_length(folders), ==, 5);
	g_hash_table_unref(seen);
	g_list_free(folders);
	gtk_widget_destroy(menu);
	g_print("PASS: visible launchers grouped once, folders and apps sorted, hidden entries omitted\n");
}

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "--menu")) {
		gtk_init(&argc, &argv);
		check_menu();
		return 0;
	}
	const struct { const char *categories, *folder; } cases[] = {
		{"Utility;Development;IDE;", "Development"},
		{"Development;Utility;", "Development"},
		{"Network;FileTransfer;Game;", "Games"},
		{"AudioVideo;Audio;Settings;", "Settings"},
		{"System;TerminalEmulator;", "Accessories"},
		{"System;FileManager;", "Accessories"},
		{"Utility;TextEditor;", "Accessories"},
		{"Office;", "Office"}, {"Graphics;", "Graphics"},
		{"AudioVideo;", "Entertainment"}, {"Network;", "Internet"},
		{"Science;", "Education"}, {"System;", "System Tools"},
		{"X-Utility;Networking;", "Other Programs"}, {"", "Other Programs"},
		{"Utility", "Accessories"},
	};
	for (guint i = 0; i < G_N_ELEMENTS(cases); i++) {
		GKeyFile *key = g_key_file_new();
		g_key_file_set_string(key, "Desktop Entry", "Type", "Application");
		g_key_file_set_string(key, "Desktop Entry", "Name", "Test application");
		g_key_file_set_string(key, "Desktop Entry", "Exec", "/bin/true");
		g_key_file_set_string(key, "Desktop Entry", "Categories", cases[i].categories);
		GDesktopAppInfo *app = g_desktop_app_info_new_from_keyfile(key);
		g_assert_nonnull(app);
		g_assert_cmpstr(app_folder(G_APP_INFO(app)), ==, cases[i].folder);
		g_object_unref(app);
		g_key_file_unref(key);
	}
	g_print("PASS: application folders, category precedence, exact tokens and missing metadata\n");
	return 0;
}
