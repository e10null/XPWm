static gboolean saw_partial_drag_frame;
static gboolean desktop_test_damage(GtkWidget *widget, cairo_t *cr, gpointer data)
{
 if (dragging) {
  double x1, y1, x2, y2; cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
  double area = (x2-x1)*(y2-y1);
  if (area > 0 && area < gtk_widget_get_allocated_width(widget)*gtk_widget_get_allocated_height(widget)/4.) saw_partial_drag_frame = TRUE;
 }
 return FALSE;
}
/* Enabled only by the private headless input harness. */
static DesktopIcon *test_icon(const char *label)
{ for (guint i = 5; i < icon_count; i++) if (!strcmp(icons[i].label, label)) return &icons[i]; return NULL; }
static GtkWidget *test_dialog(const char *title)
{
 GList *windows = gtk_window_list_toplevels(); GtkWidget *result = NULL;
 for (GList *l = windows; l; l = l->next) if (!g_strcmp0(gtk_window_get_title(GTK_WINDOW(l->data)), title)) { result = l->data; break; }
 g_list_free(windows); return result;
}
static void test_marker(const char *name, const char *text)
{ char *path = g_build_filename(g_getenv("XDG_RUNTIME_DIR"), name, NULL); g_assert_true(g_file_set_contents(path, text, -1, NULL)); g_free(path); }
static gboolean test_marked(const char *name)
{ char *path = g_build_filename(g_getenv("XDG_RUNTIME_DIR"), name, NULL); gboolean found = g_file_test(path, G_FILE_TEST_EXISTS); g_free(path); return found; }
static gboolean desktop_input_test(gpointer data)
{
 static int phase, attempts, old_x, old_y, rename_stage;
 if (++attempts > 180) g_error("Desktop input test timed out at phase %d", phase);
 if (loading || operation_cancel) return G_SOURCE_CONTINUE;
 DesktopIcon *icon;
 switch (phase) {
 case 0: {
  char *folder = g_file_get_path(desktop_folder); g_assert_true(g_str_has_prefix(folder, g_getenv("XDG_RUNTIME_DIR"))); g_free(folder);
  if (!(icon = test_icon("Sample.txt"))) break;
  g_assert_cmpuint(icon_count, ==, 7); old_x = icon->x; old_y = icon->y;
  char *coords = g_strdup_printf("%d %d", icon->x+30, icon->y+12); test_marker("desktop-target", coords); g_free(coords); phase++; break;
 }
 case 1: {
  GtkWidget *dialog = test_dialog("Rename"); if (!dialog || !gtk_window_is_active(GTK_WINDOW(dialog))) break;
  GtkWidget *entry = g_object_get_data(G_OBJECT(dialog), "name-entry"); g_assert_cmpstr(gtk_entry_get_text(GTK_ENTRY(entry)), ==, "Sample.txt");
  if (!rename_stage) {
   gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL); g_assert_nonnull(test_icon("Sample.txt"));
   rename_selected(NULL, NULL); rename_stage = 1; break;
  }
  gtk_entry_set_text(GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), "name-entry")), "Renamed.txt");
  gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK); phase++; break;
 }
 case 2:
  if (!(icon = test_icon("Renamed.txt"))) break;
  g_assert_null(test_icon("Sample.txt")); g_assert_true(icon->selected);
  g_assert_cmpint(icon->x, ==, old_x); g_assert_cmpint(icon->y, ==, old_y);
  test_marker("desktop-drag", "ready"); phase++; break;
 case 3: {
  if (!test_marked("desktop-drag-complete") || pointer_down) break;
  g_assert_true(saw_partial_drag_frame);
  icon = test_icon("Renamed.txt"); g_assert_nonnull(icon); g_assert_cmpint(icon->x, ==, old_x+96);
  char *path = layout_path(), *group = icon_group(icon); GKeyFile *saved = g_key_file_new();
  g_assert_true(g_key_file_load_from_file(saved, path, G_KEY_FILE_NONE, NULL));
  g_assert_cmpint(g_key_file_get_integer(saved, group, "x", NULL), ==, icon->x);
  g_key_file_unref(saved); g_free(path); g_free(group);
  test_marker("desktop-rubber", "ready"); phase++; break;
 }
 case 4: {
  if (!test_marked("desktop-rubber-complete") || pointer_down) break;
  GPtrArray *files = selected_files(); g_assert_cmpuint(files->len, ==, 2); g_ptr_array_unref(files);
  for (guint i = 0; i < icon_count; i++) g_assert_true(icons[i].selected);
  test_marker("desktop-copy-ready", "ready"); phase++; break;
 }
 case 5:
  if (!test_icon("Copy of Renamed.txt") || !test_icon("Copy of Folder")) break;
  g_assert_cmpuint(icon_count, ==, 9);
  clear_selection(); icon = test_icon("Copy of Renamed.txt"); icon->selected = TRUE;
  trash_selected(NULL, NULL);
  { GList *windows = gtk_window_list_toplevels(); GtkWidget *question = NULL;
   for (GList *l = windows; l; l = l->next) if (GTK_IS_MESSAGE_DIALOG(l->data)) { question = l->data; break; }
   g_assert_nonnull(question); gtk_dialog_response(GTK_DIALOG(question), GTK_RESPONSE_NO); g_list_free(windows);
  }
  g_assert_nonnull(test_icon("Copy of Renamed.txt"));
  new_folder(NULL, NULL); phase++; break;
 case 6: {
  if (!test_icon("New Folder")) break;
  GtkWidget *toggle = gtk_check_menu_item_new(); g_object_ref_sink(toggle);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(toggle), TRUE); toggle_arrange(GTK_CHECK_MENU_ITEM(toggle), NULL);
  for (guint i = 0; i < icon_count; i++) for (guint j = 0; j < i; j++)
   g_assert_true(icons[i].x >= icons[j].x+80 || icons[i].x+80 <= icons[j].x || icons[i].y >= icons[j].y+76 || icons[i].y+76 <= icons[j].y);
  gtk_widget_destroy(toggle); g_object_unref(toggle);
  GFile *external = g_file_get_child(desktop_folder, "External.txt");
  g_assert_true(g_file_replace_contents(external, "external", 8, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, NULL)); g_object_unref(external);
  phase++; break;
 }
 case 7:
  if (!test_icon("External.txt")) break;
  {
   GtkWidget *menu = arrange_menu();
   GList *items = gtk_container_get_children(GTK_CONTAINER(menu));
   GtkWidget *horizontal = g_list_last(items)->data;
   GtkWidget *vertical = g_list_last(items)->prev->data;
   gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(horizontal), TRUE);
   g_assert_true(horizontal_icons);
   g_assert_cmpint(icons[0].y, ==, icons[1].y);
   g_assert_cmpint(icons[1].x, ==, icons[0].x+96);
   char *path = layout_path(); GKeyFile *saved = g_key_file_new();
   g_assert_true(g_key_file_load_from_file(saved, path, G_KEY_FILE_NONE, NULL));
   char *alignment = g_key_file_get_string(saved, "Desktop", "IconAlignment", NULL);
   g_assert_cmpstr(alignment, ==, "Horizontal"); g_free(alignment);
   focused_icon = 0; GdkEventKey right = {.keyval = GDK_KEY_Right};
   desktop_key(canvas, &right, NULL); g_assert_cmpint(focused_icon, ==, 1);
   gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(vertical), TRUE);
   g_assert_false(horizontal_icons);
   g_assert_cmpint(icons[0].x, ==, icons[1].x);
   g_assert_cmpint(icons[1].y, ==, icons[0].y+80);
   int x, y; grid_position(grid_rows(), &x, &y);
   g_assert_cmpint(x, ==, 112); g_assert_cmpint(y, ==, 18);
   g_key_file_unref(saved); g_free(path); g_list_free(items); gtk_widget_destroy(menu);
   g_print("PASS: vertical/horizontal icon arrangement, saved preference and orientation-aware arrow navigation\n");
  }
  if (g_getenv("XPWM_DESKTOP_FILES_SCREENSHOT")) {
   char *argv[] = {"grim", (char *)g_getenv("XPWM_DESKTOP_FILES_SCREENSHOT"), NULL};
   g_assert_true(g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL));
  }
  test_marker("desktop-input-passed", "passed");
  g_print("PASS: native desktop click-to-focus/F2, rename Cancel/OK, partial-frame dragging and saved positions, rubber selection, real clipboard copy/paste, trash Cancel, New Folder, auto-arrange and live folder monitoring\n");
  gtk_main_quit(); return G_SOURCE_REMOVE;
 }
 return G_SOURCE_CONTINUE;
}
