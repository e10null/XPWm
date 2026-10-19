/* Enabled only by tests/taskbar-groups.sh in an isolated session. */
static void group_marker(const char *name)
{
 char *path = g_build_filename(g_getenv("XDG_RUNTIME_DIR"), name, NULL);
 g_assert_true(g_file_set_contents(path, "passed", -1, NULL)); g_free(path);
}
static gboolean group_test(gpointer data)
{
 static guint phase, attempts;
 static Task *target;
 static gboolean capture_wait;
 if (++attempts > 140) g_error("Taskbar group test timed out at phase %u", phase);
 if (g_list_length(tasks) < (phase >= 6 ? 1 : 5) || !gtk_widget_get_mapped(bar)) return G_SOURCE_CONTINUE;
 switch (phase) {
 case 0: {
  GPtrArray *buttons = task_buttons();
  if (buttons->len != 2) { g_ptr_array_unref(buttons); break; }
  GPtrArray *members = g_ptr_array_index(buttons, 0);
  /* App arrival order is arbitrary; find the four-window application. */
  if (members->len != 4) members = g_ptr_array_index(buttons, 1);
  if (members->len != 4) { g_ptr_array_unref(buttons); break; }
  target = task_ref(g_ptr_array_index(members, 2));
  settings.group_similar = FALSE; GPtrArray *separate = task_buttons(); g_assert_cmpuint(separate->len, ==, 5); g_ptr_array_unref(separate);
  settings.group_similar = TRUE;
  g_assert_true(task_at(START_WIDTH+4+quick_width()+5) == g_ptr_array_index((GPtrArray *)g_ptr_array_index(buttons, 0), 0));
  char *path = g_build_filename(g_getenv("XDG_RUNTIME_DIR"), "group-position", NULL);
  guint index = members == g_ptr_array_index(buttons, 0) ? 0 : 1;
  char *coords = g_strdup_printf("%d", START_WIDTH+4+quick_width()+index*task_width()+20);
  g_assert_true(g_file_set_contents(path, coords, -1, NULL)); g_free(path); g_free(coords);
  g_ptr_array_unref(buttons); group_marker("group-click"); phase++; break;
 }
 case 1: {
  if (!task_menu) break;
  if (phase == 1 && g_getenv("XPWM_GROUP_SCREENSHOT")) {
   if (!capture_wait) { capture_wait = TRUE; return G_SOURCE_CONTINUE; }
   char *argv[] = {"grim", (char *)g_getenv("XPWM_GROUP_SCREENSHOT"), NULL};
   g_assert_true(g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL));
  }
  GList *items = gtk_container_get_children(GTK_CONTAINER(task_menu)); g_assert_cmpuint(g_list_length(items), ==, 4);
  GtkWidget *chosen = NULL;
  for (GList *l = items; l; l = l->next) if (g_object_get_data(G_OBJECT(l->data), "target") == target) chosen = l->data;
  g_assert_nonnull(chosen); gtk_menu_shell_activate_item(GTK_MENU_SHELL(task_menu), chosen, TRUE); g_list_free(items); phase++; break;
 }
 case 2: {
  if (!target->active) break;
  GPtrArray *buttons = task_buttons(), *members = NULL;
  for (guint i = 0; i < buttons->len; i++) if (((GPtrArray *)g_ptr_array_index(buttons, i))->len == 4) members = g_ptr_array_index(buttons, i);
  g_assert_nonnull(members);
  GPtrArray *owned = task_members(g_ptr_array_index(members, 0));
  GtkWidget *menu = group_menu_new(owned, TRUE); g_object_ref_sink(menu);
  GList *items = gtk_container_get_children(GTK_CONTAINER(menu)); gtk_menu_item_activate(GTK_MENU_ITEM(g_list_nth_data(items, 1)));
  g_list_free(items); gtk_widget_destroy(menu); g_object_unref(menu); g_ptr_array_unref(owned); g_ptr_array_unref(buttons); phase++; break;
 }
 case 3: {
  guint minimized = 0; for (GList *l = tasks; l; l = l->next) if (((Task *)l->data)->minimized) minimized++;
  if (minimized != 4) break;
  group_marker("group-restore-click"); phase++; break;
 }
 case 4: {
  if (!task_menu) break;
  GList *items = gtk_container_get_children(GTK_CONTAINER(task_menu));
  GtkWidget *restore = g_list_nth_data(items, 0); g_assert_true(gtk_widget_get_sensitive(restore));
  gtk_menu_shell_activate_item(GTK_MENU_SHELL(task_menu), restore, TRUE); g_list_free(items); phase++; break;
 }
 case 5: {
  for (GList *l = tasks; l; l = l->next) if (((Task *)l->data)->minimized) return G_SOURCE_CONTINUE;
  group_marker("group-close-click"); phase++; break;
 }
 case 6: {
  if (!task_menu) break;
  GList *items = gtk_container_get_children(GTK_CONTAINER(task_menu));
  g_assert_false(gtk_widget_get_sensitive(g_list_nth_data(items, 0)));
  gtk_menu_shell_activate_item(GTK_MENU_SHELL(task_menu), g_list_nth_data(items, 3), TRUE); g_list_free(items); phase++; break;
 }
 case 7:
  if (g_list_length(tasks) != 1) break;
  g_assert_cmpstr(((Task *)tasks->data)->app_id, ==, "xpwm-unrelated");
  g_assert_null(target->handle); task_unref(target); target = NULL;
  group_marker("group-passed");
  g_print("PASS: real crowded taskbar hit regions, pointer-opened group list, member activation, Minimize/Restore/Close Group and unrelated-window preservation\n");
  gtk_main_quit(); return G_SOURCE_REMOVE;
 }
 return G_SOURCE_CONTINUE;
}
