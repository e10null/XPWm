/* Desktop folder model, persisted icon layout, file jobs and native interactions. */
static GFile *desktop_folder;
static GFileMonitor *desktop_monitor;
static GKeyFile *desktop_layout;
static GCancellable *list_cancel, *operation_cancel;
static GtkWidget *operation_window;
static GHashTable *select_after;
static GdkDragContext *operation_drag;
static guint32 operation_drag_time;
static guint reload_timer;
static gboolean loading, reload_again, auto_arrange, align_grid = TRUE;
static int sort_order;
static void reload_icons(void);
static void save_layout(void);
static void place_icons(gboolean force);
static char *layout_path(void) { return g_build_filename(g_get_user_config_dir(), "xpwm", "desktop.ini", NULL); }
static char *icon_group(const DesktopIcon *icon)
{
 char *hash = g_compute_checksum_for_string(G_CHECKSUM_SHA256, icon->uri, -1);
 char *group = g_strconcat("Icon-", hash, NULL); g_free(hash); return group;
}
static void free_icon(DesktopIcon *icon)
{
 g_free(icon->label); g_free(icon->uri); g_clear_object(&icon->file); g_clear_object(&icon->pixbuf);
 if (icon->rendered) cairo_surface_destroy(icon->rendered);
}
static void save_layout(void)
{
 for (guint i = 0; i < icon_count; i++) {
  char *group = icon_group(&icons[i]);
  g_key_file_set_integer(desktop_layout, group, "x", icons[i].x);
  g_key_file_set_integer(desktop_layout, group, "y", icons[i].y); g_free(group);
 }
 g_key_file_set_boolean(desktop_layout, "Desktop", "AutoArrange", auto_arrange);
 g_key_file_set_boolean(desktop_layout, "Desktop", "AlignToGrid", align_grid);
 g_key_file_set_integer(desktop_layout, "Desktop", "SortOrder", sort_order);
 char *path = layout_path(), *dir = g_path_get_dirname(path), *text = g_key_file_to_data(desktop_layout, NULL, NULL);
 GError *error = NULL;
 if (g_mkdir_with_parents(dir, 0700) || !g_file_set_contents(path, text, -1, &error)) {
  if (!error) error = g_error_new_literal(G_FILE_ERROR, G_FILE_ERROR_ACCES, "Unable to save the desktop layout.");
  error_dialog("Unable to save icon positions", error);
 }
 g_free(path); g_free(dir); g_free(text);
}
static gboolean vacant(int x, int y, guint before)
{
 for (guint i = 0; i < before; i++)
  if (x < icons[i].x+80 && x+80 > icons[i].x && y < icons[i].y+76 && y+76 > icons[i].y) return FALSE;
 return TRUE;
}
static int grid_rows(void) { return MAX(1, (gtk_widget_get_allocated_height(canvas)-18)/80); }
static void place_icons(gboolean force)
{
 int width = MAX(96, gtk_widget_get_allocated_width(canvas)), height = MAX(96, gtk_widget_get_allocated_height(canvas));
 int rows = grid_rows();
 for (guint i = 0; i < icon_count; i++) {
  DesktopIcon *icon = &icons[i];
  if (force || auto_arrange || !icon->positioned) {
   guint slot = force || auto_arrange ? i : 0;
   while (!vacant(16+(slot/rows)*96, 18+(slot%rows)*80, i)) slot++;
   icon->x = 16+(slot/rows)*96; icon->y = 18+(slot%rows)*80; icon->positioned = TRUE;
  } else {
   icon->x = CLAMP(icon->x, 0, width-80); icon->y = CLAMP(icon->y, 0, height-76);
   if (align_grid) {
    icon->x = 16+CLAMP((int)round((icon->x-16)/96.), 0, MAX(0,(width-96)/96))*96;
    icon->y = 18+CLAMP((int)round((icon->y-18)/80.), 0, MAX(0,(height-94)/80))*80;
    if (!vacant(icon->x, icon->y, i)) { icon->positioned = FALSE; i--; continue; }
   }
  }
 }
 gtk_widget_queue_draw(canvas);
}
static void restore_position(DesktopIcon *icon)
{
 char *group = icon_group(icon);
 if (g_key_file_has_key(desktop_layout, group, "x", NULL) && g_key_file_has_key(desktop_layout, group, "y", NULL)) {
  icon->x = g_key_file_get_integer(desktop_layout, group, "x", NULL);
  icon->y = g_key_file_get_integer(desktop_layout, group, "y", NULL); icon->positioned = TRUE;
 }
 g_free(group);
}
static int compare_icons(gconstpointer a, gconstpointer b)
{
 const DesktopIcon *x = a, *y = b;
 if (sort_order == 1 && x->size != y->size) return x->size < y->size ? -1 : 1;
 if (sort_order == 2) { int kind = strcmp(x->kind, y->kind); if (kind) return kind; }
 if (sort_order == 3 && x->modified != y->modified) return x->modified > y->modified ? -1 : 1;
 return g_utf8_collate(x->label, y->label);
}
static void list_worker(GTask *task, gpointer source, gpointer data, GCancellable *cancel)
{
 GError *error = NULL; GPtrArray *items = desktop_files_list(data, cancel, &error);
 if (error && !g_error_matches(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) g_task_return_error(task, error);
 else { g_clear_error(&error); if (!items) items = g_ptr_array_new_with_free_func(desktop_file_free); g_task_return_pointer(task, items, (GDestroyNotify)g_ptr_array_unref); }
}
static gboolean delayed_reload(gpointer data) { reload_timer = 0; reload_icons(); return G_SOURCE_REMOVE; }
static void folder_changed(GFileMonitor *monitor, GFile *file, GFile *other, GFileMonitorEvent event, gpointer data)
{
 if (!reload_timer) reload_timer = g_timeout_add(120, delayed_reload, NULL);
}
static void list_ready(GObject *source, GAsyncResult *result, gpointer data)
{
 GError *error = NULL; GPtrArray *items = g_task_propagate_pointer(G_TASK(result), &error);
 loading = FALSE; g_clear_object(&list_cancel);
 if (desktop_dead) { g_clear_error(&error); if (items) g_ptr_array_unref(items); return; }
 if (error) { error_dialog("Unable to read the Desktop folder", error); return; }
 if (pointer_down) { g_ptr_array_unref(items); reload_again = TRUE; return; }
 GHashTable *selected = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
 for (guint i = 5; i < icon_count; i++) {
  if (icons[i].selected) g_hash_table_add(selected, g_strdup(icons[i].uri));
  free_icon(&icons[i]);
 }
 icons = g_renew(DesktopIcon, icons, 5+items->len); icon_count = 5;
 gboolean selecting_new = select_after && g_hash_table_size(select_after);
 if (selecting_new) clear_selection();
 for (guint i = 0; i < items->len; i++) {
  DesktopFile *file = g_ptr_array_index(items, i); DesktopIcon *icon = &icons[icon_count++];
  *icon = (DesktopIcon){.label=g_strdup(file->label), .uri=g_file_get_uri(file->file), .kind=file->directory ? "folder" : "file", .file=g_object_ref(file->file), .size=file->size, .modified=file->modified};
  icon->selected = selecting_new ? g_hash_table_contains(select_after, icon->uri) : g_hash_table_contains(selected, icon->uri);
  restore_position(icon);
  if (file->icon) {
   GtkIconInfo *info = gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_default(), file->icon, 32, GTK_ICON_LOOKUP_FORCE_SIZE);
   if (info) { icon->pixbuf = gtk_icon_info_load_icon(info, NULL); g_object_unref(info); }
  }
 }
 g_hash_table_unref(selected); if (select_after) g_hash_table_remove_all(select_after);
 g_ptr_array_unref(items);
 qsort(icons+5, icon_count-5, sizeof(*icons), compare_icons);
 focused_icon = CLAMP(focused_icon, 0, (int)icon_count-1);
 selection_anchor = CLAMP(selection_anchor, 0, (int)icon_count-1); place_icons(FALSE);
 for (guint i = 0; i < icon_count; i++) if (icons[i].selected) { focused_icon = i; break; }
 save_layout();
 if (reload_again) { reload_again = FALSE; reload_icons(); }
}
static void reload_icons(void)
{
 if (desktop_dead) return;
 if (loading || pointer_down) { reload_again = TRUE; return; }
 if (!desktop_monitor) {
  desktop_monitor = g_file_monitor_directory(desktop_folder, G_FILE_MONITOR_WATCH_MOVES, NULL, NULL);
  if (desktop_monitor) g_signal_connect(desktop_monitor, "changed", G_CALLBACK(folder_changed), NULL);
 }
 loading = TRUE; list_cancel = g_cancellable_new();
 GTask *task = g_task_new(window, list_cancel, list_ready, NULL);
 g_task_set_task_data(task, g_object_ref(desktop_folder), g_object_unref);
 g_task_run_in_thread(task, list_worker); g_object_unref(task);
}
static GPtrArray *selected_files(void)
{
 GPtrArray *files = g_ptr_array_new_with_free_func(g_object_unref);
 for (guint i = 5; i < icon_count; i++) if (icons[i].selected) g_ptr_array_add(files, g_object_ref(icons[i].file));
 return files;
}
static void operation_response(GtkDialog *dialog, int response, gpointer data)
{ if (operation_cancel) g_cancellable_cancel(operation_cancel); }
static void operation_worker(GTask *task, gpointer source, gpointer data, GCancellable *cancel)
{
 DesktopJob *job = data; GError *error = NULL;
 /* Creating Desktop happens only on an explicit file operation, not on login. */
 if (job->create_destination) {
  if (!g_file_make_directory_with_parents(job->destination, cancel, &error) && !g_error_matches(error, G_IO_ERROR, G_IO_ERROR_EXISTS)) { g_task_return_error(task, error); return; }
  g_clear_error(&error);
 }
 if (desktop_job_execute(job, cancel, &error)) g_task_return_boolean(task, TRUE);
 else g_task_return_error(task, error);
}
static void operation_ready(GObject *source, GAsyncResult *result, gpointer data)
{
 GError *error = NULL; gboolean ok = g_task_propagate_boolean(G_TASK(result), &error);
 DesktopJob *job = g_task_get_task_data(G_TASK(result));
 g_clear_object(&operation_cancel);
 if (operation_drag) { gtk_drag_finish(operation_drag, ok, FALSE, operation_drag_time); g_clear_object(&operation_drag); }
 if (desktop_dead) { g_clear_error(&error); return; }
 if (operation_window) gtk_widget_destroy(operation_window);
 operation_window = NULL;
 if (!select_after) select_after = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
 for (guint i = 0; i < job->results->len; i++) g_hash_table_add(select_after, g_file_get_uri(g_ptr_array_index(job->results, i)));
 if (job->operation == DESKTOP_RENAME && job->results->len && job->sources->len) {
  char *old_uri = g_file_get_uri(g_ptr_array_index(job->sources, 0));
  for (guint i = 5; i < icon_count; i++) if (!strcmp(icons[i].uri, old_uri)) {
   DesktopIcon renamed = {.uri = g_file_get_uri(g_ptr_array_index(job->results, 0))};
   char *group = icon_group(&renamed); g_key_file_set_integer(desktop_layout, group, "x", icons[i].x); g_key_file_set_integer(desktop_layout, group, "y", icons[i].y);
   g_free(group); g_free(renamed.uri); break;
  }
  g_free(old_uri);
 }
 /* Keep clipboard ownership intact if another application changed it during the job. */
 if (error && !g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
  char *message = g_strdup_printf("File operation stopped after %u item(s)", job->completed);
  error_dialog(message, error); g_free(message);
 } else g_clear_error(&error);
 reload_icons();
}
static gboolean start_operation(DesktopJob *job)
{
 if (operation_cancel) { desktop_job_free(job); return FALSE; }
 job->create_destination = job->destination && g_file_equal(job->destination, desktop_folder);
 operation_cancel = g_cancellable_new();
 operation_window = gtk_dialog_new_with_buttons("File operation", GTK_WINDOW(window), GTK_DIALOG_DESTROY_WITH_PARENT, "Cancel", GTK_RESPONSE_CANCEL, NULL);
 const char *verbs[] = {"Copying files…", "Moving files…", "Moving items to the Recycle Bin…", "Renaming item…", "Creating a new folder…"};
 GtkWidget *label = gtk_label_new(verbs[job->operation]); gtk_widget_set_margin_top(label, 18); gtk_widget_set_margin_bottom(label, 18);
 gtk_widget_set_margin_start(label, 18); gtk_widget_set_margin_end(label, 18);
 gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(operation_window))), label);
 g_signal_connect(operation_window, "response", G_CALLBACK(operation_response), NULL);
 g_signal_connect(operation_window, "destroy", G_CALLBACK(gtk_widget_destroyed), &operation_window);
 xp_style_apply(operation_window); gtk_widget_show_all(operation_window);
 GTask *task = g_task_new(window, operation_cancel, operation_ready, NULL);
 g_task_set_check_cancellable(task, FALSE); g_task_set_task_data(task, job, desktop_job_free);
 g_task_run_in_thread(task, operation_worker); g_object_unref(task); return TRUE;
}
static void new_folder(GtkMenuItem *item, gpointer data) { start_operation(desktop_job_new(DESKTOP_NEW_FOLDER, desktop_folder)); }
static void rename_response(GtkDialog *dialog, int response, gpointer data)
{
 if (response == GTK_RESPONSE_OK) {
  GtkWidget *entry = g_object_get_data(G_OBJECT(dialog), "name-entry");
  DesktopJob *job = desktop_job_new(DESKTOP_RENAME, NULL);
  g_ptr_array_add(job->sources, g_object_ref(g_object_get_data(G_OBJECT(dialog), "source-file")));
  job->name = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry))); start_operation(job);
 }
 gtk_widget_destroy(GTK_WIDGET(dialog));
}
static void rename_selected(GtkMenuItem *item, gpointer data)
{
 GPtrArray *files = selected_files();
 if (files->len != 1 || operation_cancel) { g_ptr_array_unref(files); return; }
 GFile *file = g_ptr_array_index(files, 0);
 GtkWidget *dialog = gtk_dialog_new_with_buttons("Rename", GTK_WINDOW(window), GTK_DIALOG_DESTROY_WITH_PARENT, "OK", GTK_RESPONSE_OK, "Cancel", GTK_RESPONSE_CANCEL, NULL);
 GtkWidget *entry = gtk_entry_new(); char *name = g_file_get_basename(file), *display = g_filename_display_name(name);
 gtk_entry_set_text(GTK_ENTRY(entry), display); gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE); gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
 gtk_container_set_border_width(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 12);
 gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), entry);
 g_object_set_data(G_OBJECT(dialog), "name-entry", entry); g_object_set_data_full(G_OBJECT(dialog), "source-file", g_object_ref(file), g_object_unref);
 g_signal_connect(dialog, "response", G_CALLBACK(rename_response), NULL); xp_style_apply(dialog); gtk_widget_show_all(dialog); gtk_widget_grab_focus(entry);
 char *dot = strrchr(display, '.'); gtk_editable_select_region(GTK_EDITABLE(entry), 0, dot && dot != display ? g_utf8_pointer_to_offset(display, dot) : -1);
 g_free(name); g_free(display); g_ptr_array_unref(files);
}
static void trash_response(GtkDialog *dialog, int response, gpointer data)
{
 if (response == GTK_RESPONSE_YES) {
  GPtrArray *files = g_object_get_data(G_OBJECT(dialog), "files"); DesktopJob *job = desktop_job_new(DESKTOP_TRASH, NULL);
  for (guint i = 0; i < files->len; i++) g_ptr_array_add(job->sources, g_object_ref(g_ptr_array_index(files, i)));
  start_operation(job);
 }
 gtk_widget_destroy(GTK_WIDGET(dialog));
}
static void trash_selected(GtkMenuItem *item, gpointer data)
{
 GPtrArray *files = selected_files(); if (!files->len || operation_cancel) { g_ptr_array_unref(files); return; }
 GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
  "Move %u item(s) to the Recycle Bin?", files->len);
 gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_NO);
 g_object_set_data_full(G_OBJECT(dialog), "files", files, (GDestroyNotify)g_ptr_array_unref);
 g_signal_connect(dialog, "response", G_CALLBACK(trash_response), NULL); xp_style_apply(dialog); gtk_widget_show(dialog);
}
/* Standard file-manager clipboard data; the desktop owns a snapshot of URIs. */
typedef struct { char *text; char **uris; } DesktopClipboard;
static void clipboard_get(GtkClipboard *clipboard, GtkSelectionData *selection, guint info, gpointer data)
{
 DesktopClipboard *files = data;
 if (info == 0) gtk_selection_data_set(selection, gtk_selection_data_get_target(selection), 8, (const guchar *)files->text, strlen(files->text));
 else if (info == 1) gtk_selection_data_set_uris(selection, files->uris);
 else { char *text = g_strjoinv("\n", files->uris); gtk_selection_data_set_text(selection, text, -1); g_free(text); }
}
static void clipboard_clear(GtkClipboard *clipboard, gpointer data)
{ DesktopClipboard *files = data; g_free(files->text); g_strfreev(files->uris); g_free(files); }
static void copy_selected(GtkMenuItem *item, gpointer data)
{
 GPtrArray *files = selected_files(); if (!files->len) { g_ptr_array_unref(files); return; }
 DesktopClipboard *content = g_new0(DesktopClipboard, 1); content->uris = g_new0(char *, files->len+1);
 GString *text = g_string_new(GPOINTER_TO_INT(data) ? "cut\n" : "copy\n");
 for (guint i = 0; i < files->len; i++) { content->uris[i] = g_file_get_uri(g_ptr_array_index(files, i)); g_string_append_printf(text, "%s\n", content->uris[i]); }
 content->text = g_string_free(text, FALSE);
 GtkTargetEntry targets[] = {{"x-special/gnome-copied-files", 0, 0}, {"text/uri-list", 0, 1}, {"UTF8_STRING", 0, 2}};
 if (!gtk_clipboard_set_with_data(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), targets, G_N_ELEMENTS(targets), clipboard_get, clipboard_clear, content)) clipboard_clear(NULL, content);
 g_ptr_array_unref(files);
}
static DesktopJob *uri_job(char **uris, gboolean move, GFile *destination)
{
 DesktopJob *job = desktop_job_new(move ? DESKTOP_MOVE : DESKTOP_COPY, destination);
 for (guint i = 0; uris && uris[i]; i++) {
  if (!g_str_has_prefix(uris[i], "file://")) continue;
  GFile *file = g_file_new_for_uri(uris[i]);
  if (g_file_is_native(file)) g_ptr_array_add(job->sources, file); else g_object_unref(file);
 }
 return job;
}
static void paste_uris(GtkClipboard *clipboard, gchar **uris, gpointer data)
{
 if (!desktop_dead) { DesktopJob *job = uri_job(uris, FALSE, data); if (job->sources->len) start_operation(job); else desktop_job_free(job); }
 g_object_unref(data);
}
static void paste_received(GtkClipboard *clipboard, GtkSelectionData *selection, gpointer data)
{
 if (desktop_dead) { g_object_unref(data); return; }
 int length = gtk_selection_data_get_length(selection);
 if (length <= 0) { gtk_clipboard_request_uris(clipboard, paste_uris, data); return; }
 char *text = g_strndup((const char *)gtk_selection_data_get_data(selection), length); char **lines = g_strsplit(text, "\n", -1);
 DesktopJob *job = uri_job(lines+1, g_str_has_prefix(text, "cut\n"), data);
 if (job->sources->len) start_operation(job); else desktop_job_free(job);
 g_strfreev(lines); g_free(text); g_object_unref(data);
}
static void paste_files(GtkMenuItem *item, gpointer data)
{
 GFile *destination = item ? g_object_get_data(G_OBJECT(item), "destination") : NULL;
 gtk_clipboard_request_contents(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), gdk_atom_intern_static_string("x-special/gnome-copied-files"), paste_received, g_object_ref(destination ? destination : desktop_folder));
}
static void sort_icons(GtkMenuItem *item, gpointer data)
{ sort_order = GPOINTER_TO_INT(data); qsort(icons+5, icon_count-5, sizeof(*icons), compare_icons); place_icons(TRUE); save_layout(); }
static void toggle_arrange(GtkCheckMenuItem *item, gpointer data)
{ auto_arrange = gtk_check_menu_item_get_active(item); if (auto_arrange) place_icons(TRUE); save_layout(); }
static void toggle_grid(GtkCheckMenuItem *item, gpointer data)
{ align_grid = gtk_check_menu_item_get_active(item); place_icons(FALSE); save_layout(); }
static void open_selected(GtkMenuItem *item, gpointer data)
{ if (focused_icon >= 0 && (guint)focused_icon < icon_count) launch(&icons[focused_icon], gtk_get_current_event_time()); }
static GtkWidget *action(GtkWidget *menu, const char *name, GCallback callback, gpointer data, gboolean sensitive)
{
 GtkWidget *item = gtk_menu_item_new_with_label(name); gtk_widget_set_sensitive(item, sensitive);
 g_signal_connect(item, "activate", callback, data); gtk_menu_shell_append(GTK_MENU_SHELL(menu), item); return item;
}
static gboolean dispose_desktop_menu(gpointer data) { gtk_widget_destroy(data); g_object_unref(data); return G_SOURCE_REMOVE; }
static void destroy_menu(GtkWidget *menu, gpointer data) {
 if (!g_object_get_data(G_OBJECT(menu), "dispose-pending")) {
  g_object_set_data(G_OBJECT(menu), "dispose-pending", GINT_TO_POINTER(1));
  g_idle_add(dispose_desktop_menu, g_object_ref(menu));
 }
}
static GtkWidget *arrange_menu(void)
{
 GtkWidget *menu = gtk_menu_new(); const char *names[] = {"Name", "Size", "Type", "Modified"};
 for (guint i = 0; i < G_N_ELEMENTS(names); i++) action(menu, names[i], G_CALLBACK(sort_icons), GINT_TO_POINTER(i), TRUE);
 gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
 GtkWidget *item = gtk_check_menu_item_new_with_label("Auto Arrange"); gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), auto_arrange);
 g_signal_connect(item, "toggled", G_CALLBACK(toggle_arrange), NULL); gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
 item = gtk_check_menu_item_new_with_label("Align to Grid"); gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), align_grid);
 g_signal_connect(item, "toggled", G_CALLBACK(toggle_grid), NULL); gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
 xp_style_apply(menu); return menu;
}
static void canvas_resized(GtkWidget *widget, GtkAllocation *allocation, gpointer data) { if (!pointer_down) place_icons(FALSE); }
static void damage_icon(const DesktopIcon *icon)
{ gtk_widget_queue_draw_area(canvas, icon->x-2, icon->y-2, 84, 80); }
static void damage_rubber(double end_x, double end_y)
{
 int x = floor(MIN(press_x, end_x))-2, y = floor(MIN(press_y, end_y))-2;
 int right = ceil(MAX(press_x, end_x))+2, bottom = ceil(MAX(press_y, end_y))+2;
 gtk_widget_queue_draw_area(canvas, x, y, right-x, bottom-y);
}
static gboolean desktop_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
 if (!pointer_down) return FALSE;
 int dx = (int)round(event->x-press_x), dy = (int)round(event->y-press_y);
 if (rubber_select) {
  if (rubber_x == event->x && rubber_y == event->y) return TRUE;
  damage_rubber(rubber_x, rubber_y);
  rubber_x = event->x; rubber_y = event->y;
  damage_rubber(rubber_x, rubber_y);
  int x = MIN(press_x, rubber_x), y = MIN(press_y, rubber_y), w = abs(dx), h = abs(dy);
  for (guint i = 0; i < icon_count; i++) {
   gboolean inside = x < icons[i].x+80 && x+w > icons[i].x && y < icons[i].y+76 && y+h > icons[i].y;
   gboolean base = selection_base && selection_base[i];
   gboolean selected = rubber_toggle ? base != inside : rubber_add ? base || inside : inside;
   if (icons[i].selected != selected) { icons[i].selected = selected; damage_icon(&icons[i]); }
  }
 } else if (!auto_arrange && drag_start && (dragging || abs(dx) > 4 || abs(dy) > 4)) {
  dragging = TRUE;
  int minx = G_MAXINT, miny = G_MAXINT, maxx = 0, maxy = 0;
  for (guint i = 0; i < icon_count; i++) if (icons[i].selected) { minx = MIN(minx, drag_start[i*2]); miny = MIN(miny, drag_start[i*2+1]); maxx = MAX(maxx, drag_start[i*2]+80); maxy = MAX(maxy, drag_start[i*2+1]+76); }
  if (minx != G_MAXINT) {
   dx = CLAMP(dx, -minx, MAX(-minx, gtk_widget_get_allocated_width(canvas)-maxx));
   dy = CLAMP(dy, -miny, MAX(-miny, gtk_widget_get_allocated_height(canvas)-maxy));
   for (guint i = 0; i < icon_count; i++) if (icons[i].selected) {
    int x = drag_start[i*2]+dx, y = drag_start[i*2+1]+dy;
    if (icons[i].x == x && icons[i].y == y) continue;
    damage_icon(&icons[i]); icons[i].x = x; icons[i].y = y; damage_icon(&icons[i]);
   }
  }
 }
 /* GTK coalesces the old/new damage into its next frame; no full redraw or
  * layout persistence is needed for an unchanged pointer position. */
 return TRUE;
}
static gboolean desktop_release(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
 if (event->button != 1 || !pointer_down) return FALSE;
 pointer_down = FALSE;
 if (plain_click && !dragging && !rubber_select) { clear_selection(); icons[focused_icon].selected = TRUE; }
 if (dragging) { place_icons(FALSE); save_layout(); }
 dragging = rubber_select = FALSE; g_clear_pointer(&drag_start, g_free); g_clear_pointer(&selection_base, g_free);
 gtk_widget_queue_draw(canvas);
 if (reload_again) { reload_again = FALSE; reload_icons(); }
 return TRUE;
}
static gboolean desktop_key(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
 gboolean control = event->state & GDK_CONTROL_MASK;
 if (control && (event->keyval == GDK_KEY_a || event->keyval == GDK_KEY_A)) { for (guint i = 0; i < icon_count; i++) icons[i].selected = TRUE; }
 else if (control && (event->keyval == GDK_KEY_c || event->keyval == GDK_KEY_C)) copy_selected(NULL, NULL);
 else if (control && (event->keyval == GDK_KEY_x || event->keyval == GDK_KEY_X)) copy_selected(NULL, GINT_TO_POINTER(1));
 else if (control && (event->keyval == GDK_KEY_v || event->keyval == GDK_KEY_V)) paste_files(NULL, NULL);
 else if (event->keyval == GDK_KEY_space) { if (control) icons[focused_icon].selected = !icons[focused_icon].selected; else { clear_selection(); icons[focused_icon].selected = TRUE; } }
 else if (event->keyval == GDK_KEY_F2) rename_selected(NULL, NULL);
 else if (event->keyval == GDK_KEY_Delete) trash_selected(NULL, NULL);
 else if (event->keyval == GDK_KEY_F5) reload_icons();
 else if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) open_selected(NULL, NULL);
 else if (event->keyval == GDK_KEY_Escape) clear_selection();
 else {
  int next = focused_icon;
  if (event->keyval == GDK_KEY_Down) next++;
  else if (event->keyval == GDK_KEY_Up) next--;
  else if (event->keyval == GDK_KEY_Left) next -= grid_rows();
  else if (event->keyval == GDK_KEY_Right) next += grid_rows();
  else if (event->keyval == GDK_KEY_Home) next = 0;
  else if (event->keyval == GDK_KEY_End) next = icon_count-1;
  else return FALSE;
  focused_icon = CLAMP(next, 0, (int)icon_count-1);
  if (event->state & GDK_SHIFT_MASK) {
   if (!control) clear_selection();
   for (int i = MIN(selection_anchor, focused_icon); i <= MAX(selection_anchor, focused_icon); i++) icons[i].selected = TRUE;
  } else if (!control) { clear_selection(); icons[focused_icon].selected = TRUE; selection_anchor = focused_icon; }
 }
 gtk_widget_queue_draw(canvas); return TRUE;
}
static void desktop_drop(GtkWidget *widget, GdkDragContext *context, int x, int y, GtkSelectionData *selection, guint info, guint time, gpointer data)
{
 char **uris = gtk_selection_data_get_uris(selection); DesktopIcon *icon = hit(x, y);
 GFile *destination = icon && icon->file && !strcmp(icon->kind, "folder") ? icon->file : desktop_folder;
 DesktopJob *job = uri_job(uris, FALSE, destination); g_strfreev(uris);
 if (!job->sources->len || operation_cancel) { desktop_job_free(job); gtk_drag_finish(context, FALSE, FALSE, time); return; }
 operation_drag = g_object_ref(context); operation_drag_time = time;
 start_operation(job);
}
static void init_icons(void)
{
 desktop_layout = g_key_file_new(); char *path = layout_path(); g_key_file_load_from_file(desktop_layout, path, G_KEY_FILE_NONE, NULL); g_free(path);
 auto_arrange = g_key_file_get_boolean(desktop_layout, "Desktop", "AutoArrange", NULL);
 if (g_key_file_has_key(desktop_layout, "Desktop", "AlignToGrid", NULL)) align_grid = g_key_file_get_boolean(desktop_layout, "Desktop", "AlignToGrid", NULL);
 sort_order = CLAMP(g_key_file_get_integer(desktop_layout, "Desktop", "SortOrder", NULL), 0, 3);
 const char *documents = g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS);
 char *document_uri = g_filename_to_uri(documents ? documents : g_get_home_dir(), NULL, NULL);
 const char *labels[] = {"My Documents", "My Computer", "My Network Places", "Recycle Bin", "Internet Explorer"};
 const char *uris[] = {document_uri, "computer:///", "network:///", "trash:///", "https://www.microsoft.com/"};
 const char *kinds[] = {"folder", "computer", "network", "trash", "browser"};
 icons = g_new0(DesktopIcon, 5); icon_count = 5;
 for (guint i = 0; i < icon_count; i++) { icons[i] = (DesktopIcon){.label=g_strdup(labels[i]), .uri=g_strdup(uris[i]), .kind=kinds[i], .x=16, .y=18+(int)i*80, .positioned=TRUE}; restore_position(&icons[i]); }
 g_free(document_uri);
 const char *directory = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
 char *fallback = g_build_filename(g_get_home_dir(), "Desktop", NULL);
 desktop_folder = g_file_new_for_path(directory && strcmp(directory, g_get_home_dir()) ? directory : fallback); g_free(fallback);
}
static void desktop_destroyed(GtkWidget *widget, gpointer data)
{
 desktop_dead = TRUE;
 if (list_cancel) g_cancellable_cancel(list_cancel);
 if (operation_cancel) g_cancellable_cancel(operation_cancel);
 if (reload_timer) g_source_remove(reload_timer);
 reload_timer = 0;
 g_clear_object(&desktop_monitor);
 gtk_main_quit();
}
