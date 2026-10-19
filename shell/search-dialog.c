#define _POSIX_C_SOURCE 200809L
/* Native XP-style filename search. Filesystem traversal stays off the UI thread. */
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include "search-dialog.h"
#include "xp-style.h"

typedef struct { char *name, *folder, *path; goffset size; gboolean directory; } Hit;
typedef struct {
 char *root, *pattern, *content;
 gboolean hidden, stopped, limited, shallow;
 guint unreadable, content_skipped;
 guint64 min_size, max_size;
 gint64 after, before;
 GPtrArray *hits;
} Query;
typedef struct {
 GtkWidget *window, *name, *folder, *hidden, *start, *stop, *status, *view;
 GtkWidget *content, *recursive, *min_size, *max_size, *after, *before;
 GtkListStore *model;
 GCancellable *cancel;
 gboolean dead;
} Search;
static GtkWidget *search_window;
enum { NAME, FOLDER, SIZE, PATH, COLUMNS };
static void free_hit(gpointer data)
{
 Hit *h = data; g_free(h->name); g_free(h->folder); g_free(h->path); g_free(h);
}
static void free_query(gpointer data)
{
 Query *q = data; g_free(q->root); g_free(q->pattern); g_free(q->content); g_ptr_array_unref(q->hits); g_free(q);
}
static gboolean matches(const char *name, const char *pattern)
{
 char *utf8 = g_filename_display_name(name), *fold = g_utf8_casefold(utf8, -1);
 gboolean match = strpbrk(pattern, "*?") ? g_pattern_match_simple(pattern, fold) : strstr(fold, pattern) != NULL;
 g_free(fold); g_free(utf8); return match;
}
/* Only scan regular UTF-8 text up to 16 MiB. O_NOFOLLOW and O_NONBLOCK
 * keep symlink replacement and special files from escaping or hanging a search. */
static gboolean content_matches(Query *q, const char *path, const GStatBuf *expected, GCancellable *cancel)
{
 if (!q->content || !*q->content) return TRUE;
 if (!S_ISREG(expected->st_mode) || expected->st_size > 16*1024*1024) { q->content_skipped++; return FALSE; }
 int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
 if (fd < 0) { q->unreadable++; return FALSE; }
 struct stat actual;
 if (fstat(fd, &actual) || !S_ISREG(actual.st_mode) || actual.st_dev != expected->st_dev || actual.st_ino != expected->st_ino || actual.st_size > 16*1024*1024) {
  close(fd); q->content_skipped++; return FALSE;
 }
 GString *text = g_string_sized_new(MIN((gsize)actual.st_size, (gsize)65536)); char buffer[32768]; ssize_t n;
 gboolean valid = TRUE;
 while (!g_cancellable_is_cancelled(cancel)) {
  n = read(fd, buffer, sizeof(buffer));
  if (!n) break;
  if (n < 0) { if (errno == EINTR) continue; valid = FALSE; q->unreadable++; break; }
  if (text->len+n > 16*1024*1024 || memchr(buffer, 0, n)) { valid = FALSE; q->content_skipped++; break; }
  g_string_append_len(text, buffer, n);
 }
 close(fd);
 gboolean match = FALSE;
 if (valid && !g_cancellable_is_cancelled(cancel) && g_utf8_validate(text->str, text->len, NULL)) {
  char *fold = g_utf8_casefold(text->str, text->len); match = strstr(fold, q->content) != NULL; g_free(fold);
 } else if (valid && !g_cancellable_is_cancelled(cancel)) q->content_skipped++;
 g_string_free(text, TRUE); return match;
}
static gboolean metadata_matches(Query *q, const GStatBuf *stat)
{
 if (q->min_size || q->max_size) {
  if (!S_ISREG(stat->st_mode)) return FALSE;
  if ((guint64)stat->st_size < q->min_size || (q->max_size && (guint64)stat->st_size > q->max_size)) return FALSE;
 }
 return (!q->after || stat->st_mtime >= q->after) && (!q->before || stat->st_mtime < q->before);
}
static gboolean parse_date(const char *value, gboolean end, gint64 *result)
{
 if (!value || !*value) { *result = 0; return TRUE; }
 guint year, month, day; char extra;
 if (strlen(value) != 10 || value[4] != '-' || value[7] != '-' || sscanf(value, "%4u-%2u-%2u%c", &year, &month, &day, &extra) != 3 ||
  !g_date_valid_dmy(day, month, year)) return FALSE;
 GDateTime *date = g_date_time_new_local(year, month, day, 0, 0, 0);
 if (!date) return FALSE;
 if (end) { GDateTime *next = g_date_time_add_days(date, 1); g_date_time_unref(date); date = next; }
 *result = g_date_time_to_unix(date); g_date_time_unref(date); return TRUE;
}
static void search_worker(GTask *task, gpointer source, gpointer task_data, GCancellable *cancel)
{
 Query *q = task_data;
 GQueue pending = G_QUEUE_INIT;
 g_queue_push_tail(&pending, g_strdup(q->root));
 guint visited = 0;
 while (!g_queue_is_empty(&pending)) {
  if (g_cancellable_is_cancelled(cancel)) { q->stopped = TRUE; break; }
  if (++visited > 100000) { q->limited = TRUE; break; }
  char *folder = g_queue_pop_head(&pending);
  GDir *dir = g_dir_open(folder, 0, NULL);
  if (!dir) q->unreadable++;
  const char *name;
  while (dir && (name = g_dir_read_name(dir))) {
   if (g_cancellable_is_cancelled(cancel)) { q->stopped = TRUE; break; }
   if (!q->hidden && name[0] == '.') continue;
   char *path = g_build_filename(folder, name, NULL);
   GStatBuf stat;
   if (g_lstat(path, &stat)) { g_free(path); continue; }
   gboolean directory = S_ISDIR(stat.st_mode);
   /* lstat never traverses directory symlinks: loops and links outside the
    * selected folder cannot turn a search into an unbounded traversal. */
   if (directory && !q->shallow) {
    if (pending.length >= 100000) q->limited = TRUE;
    else g_queue_push_tail(&pending, g_strdup(path));
   }
   if (matches(name, q->pattern) && metadata_matches(q, &stat) && content_matches(q, path, &stat, cancel)) {
    Hit *h = g_new0(Hit, 1);
    h->name = g_filename_display_name(name); h->folder = g_filename_display_name(folder);
    h->path = g_strdup(path); h->size = stat.st_size; h->directory = directory;
    g_ptr_array_add(q->hits, h);
   }
   g_free(path);
   if (q->hits->len >= 1000) { q->limited = TRUE; break; }
  }
  if (dir) g_dir_close(dir);
  g_free(folder);
  if (q->limited || q->stopped) break;
 }
 if (g_cancellable_is_cancelled(cancel)) q->stopped = TRUE;
 g_queue_clear_full(&pending, g_free);
 g_task_return_boolean(task, TRUE);
}
static void search_ready(GObject *object, GAsyncResult *result, gpointer data)
{
 Search *s = g_object_get_data(object, "search");
 Query *q = g_task_get_task_data(G_TASK(result));
 g_task_propagate_boolean(G_TASK(result), NULL);
 g_clear_object(&s->cancel);
 if (s->dead) return;
 for (guint i = 0; i < q->hits->len; i++) {
  Hit *h = g_ptr_array_index(q->hits, i);
  char *size = h->directory ? g_strdup("File Folder") : g_format_size(h->size);
  GtkTreeIter iter;
  gtk_list_store_append(s->model, &iter);
  gtk_list_store_set(s->model, &iter, NAME, h->name, FOLDER, h->folder, SIZE, size, PATH, h->path, -1);
  g_free(size);
 }
 char *text = g_strdup_printf("%s: %u item%s found%s%s.", q->stopped ? "Search stopped" : "Search complete",
  q->hits->len, q->hits->len == 1 ? "" : "s", q->limited ? " (search limit reached)" : "",
  q->unreadable ? "; some items could not be read" : "");
 if (q->content_skipped) {
  char *extra = g_strdup_printf("%s %u binary, nonregular or oversized item(s) skipped.", text, q->content_skipped);
  g_free(text); text = extra;
 }
 gtk_label_set_text(GTK_LABEL(s->status), text); gtk_widget_set_tooltip_text(s->status, text); g_free(text);
 gtk_widget_set_sensitive(s->start, TRUE); gtk_widget_set_sensitive(s->stop, FALSE);
}
static void start_search(GtkWidget *widget, gpointer data)
{
 Search *s = data;
 if (s->cancel) return;
 char *root = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(s->folder));
 if (!root) { gtk_label_set_text(GTK_LABEL(s->status), "Choose a folder to search."); return; }
 gint64 after = 0, before = 0;
 if (!parse_date(gtk_entry_get_text(GTK_ENTRY(s->after)), FALSE, &after) || !parse_date(gtk_entry_get_text(GTK_ENTRY(s->before)), TRUE, &before) ||
  (after && before && after >= before)) {
  gtk_label_set_text(GTK_LABEL(s->status), "Enter a valid modified-date range using YYYY-MM-DD."); g_free(root); return;
 }
 guint64 minimum = (guint64)gtk_spin_button_get_value(GTK_SPIN_BUTTON(s->min_size))*1024;
 guint64 maximum = (guint64)gtk_spin_button_get_value(GTK_SPIN_BUTTON(s->max_size))*1024;
 if (maximum && minimum > maximum) { gtk_label_set_text(GTK_LABEL(s->status), "Maximum size must be at least the minimum size."); g_free(root); return; }
 Query *q = g_new0(Query, 1);
 q->content = g_utf8_casefold(gtk_entry_get_text(GTK_ENTRY(s->content)), -1);
 q->shallow = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s->recursive));
 q->min_size = minimum; q->max_size = maximum; q->after = after; q->before = before;
 q->root = root; q->pattern = g_utf8_casefold(gtk_entry_get_text(GTK_ENTRY(s->name)), -1);
 q->hidden = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s->hidden));
 q->hits = g_ptr_array_new_with_free_func(free_hit);
 gtk_list_store_clear(s->model);
 gtk_label_set_text(GTK_LABEL(s->status), "Searching files and folders…");
 gtk_widget_set_sensitive(s->start, FALSE); gtk_widget_set_sensitive(s->stop, TRUE);
 s->cancel = g_cancellable_new();
 GTask *task = g_task_new(s->window, s->cancel, search_ready, NULL);
 g_task_set_check_cancellable(task, FALSE); /* Show partial results when stopped. */
 g_task_set_task_data(task, q, free_query); g_task_run_in_thread(task, search_worker); g_object_unref(task);
}
static void stop_search(GtkWidget *widget, gpointer data)
{
 Search *s = data; if (s->cancel) g_cancellable_cancel(s->cancel);
}
static void open_hit(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer data)
{
 Search *s = data; GtkTreeIter iter; char *filename = NULL;
 if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(s->model), &iter, path)) return;
 gtk_tree_model_get(GTK_TREE_MODEL(s->model), &iter, PATH, &filename, -1);
 char *uri = g_filename_to_uri(filename, NULL, NULL); GError *error = NULL;
 if (uri) gtk_show_uri_on_window(GTK_WINDOW(s->window), uri, gtk_get_current_event_time(), &error);
 if (error) {
  GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(s->window), GTK_DIALOG_DESTROY_WITH_PARENT,
   GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Unable to open this item");
  gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", error->message);
  xp_style_apply(dialog); g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
  gtk_widget_show(dialog); g_error_free(error);
 }
 g_free(uri); g_free(filename);
}
static GKeyFile *search_snapshot(Search *s)
{
 GKeyFile *ini = g_key_file_new(); char *folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(s->folder));
 g_key_file_set_integer(ini, "Search", "Version", 1);
 g_key_file_set_string(ini, "Search", "Folder", folder ? folder : g_get_home_dir()); g_free(folder);
 g_key_file_set_string(ini, "Search", "Name", gtk_entry_get_text(GTK_ENTRY(s->name)));
 g_key_file_set_string(ini, "Search", "Content", gtk_entry_get_text(GTK_ENTRY(s->content)));
 g_key_file_set_string(ini, "Search", "ModifiedFrom", gtk_entry_get_text(GTK_ENTRY(s->after)));
 g_key_file_set_string(ini, "Search", "ModifiedThrough", gtk_entry_get_text(GTK_ENTRY(s->before)));
 g_key_file_set_boolean(ini, "Search", "Hidden", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s->hidden)));
 g_key_file_set_boolean(ini, "Search", "Subfolders", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s->recursive)));
 g_key_file_set_uint64(ini, "Search", "MinimumKiB", gtk_spin_button_get_value(GTK_SPIN_BUTTON(s->min_size)));
 g_key_file_set_uint64(ini, "Search", "MaximumKiB", gtk_spin_button_get_value(GTK_SPIN_BUTTON(s->max_size)));
 return ini;
}
static gboolean restore_search(Search *s, GKeyFile *ini, GError **error)
{
 if (g_key_file_get_integer(ini, "Search", "Version", NULL) != 1) {
  g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "This is not a supported XPWm saved search."); return FALSE;
 }
 const char *keys[] = {"Folder", "Name", "Content", "ModifiedFrom", "ModifiedThrough"}; char *values[5] = {0};
 gboolean ok = TRUE;
 for (guint i = 0; i < G_N_ELEMENTS(keys); i++) {
  values[i] = g_key_file_get_string(ini, "Search", keys[i], NULL);
  if (!values[i] || !g_utf8_validate(values[i], -1, NULL) || strlen(values[i]) > 65536) ok = FALSE;
 }
 guint64 min = g_key_file_get_uint64(ini, "Search", "MinimumKiB", NULL), max = g_key_file_get_uint64(ini, "Search", "MaximumKiB", NULL);
 gint64 after = 0, before = 0;
 ok = ok && min <= 1073741824 && max <= 1073741824 && (!max || min <= max) && g_path_is_absolute(values[0]) &&
  parse_date(values[3], FALSE, &after) && parse_date(values[4], TRUE, &before) && (!after || !before || after < before);
 if (ok) {
  gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(s->folder), values[0]);
  GtkWidget *entries[] = {s->name, s->content, s->after, s->before};
  for (guint i = 0; i < G_N_ELEMENTS(entries); i++) gtk_entry_set_text(GTK_ENTRY(entries[i]), values[i+1]);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->min_size), min); gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->max_size), max);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->hidden), g_key_file_get_boolean(ini, "Search", "Hidden", NULL));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->recursive), g_key_file_get_boolean(ini, "Search", "Subfolders", NULL));
 } else g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "The saved search contains invalid folder, date or size settings.");
 for (guint i = 0; i < G_N_ELEMENTS(values); i++) g_free(values[i]);
 return ok;
}
static void saved_response(GtkDialog *dialog, int response, gpointer data)
{
 GtkWindow *parent = gtk_window_get_transient_for(GTK_WINDOW(dialog));
 Search *s = parent ? g_object_get_data(G_OBJECT(parent), "search") : NULL;
 if (response == GTK_RESPONSE_ACCEPT && s && !s->dead) {
  char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)); GError *error = NULL;
  gboolean save = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "save-search"));
  GKeyFile *ini = save ? search_snapshot(s) : g_key_file_new();
  gboolean ok = filename != NULL;
  if (ok && save) {
   char *text = g_key_file_to_data(ini, NULL, NULL); ok = g_file_set_contents(filename, text, -1, &error); g_free(text);
  } else if (ok) {
   GFile *file = g_file_new_for_path(filename); GFileInfo *info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_SIZE, 0, NULL, &error);
   ok = info && g_file_info_get_size(info) <= 1024*1024;
   if (!ok && !error) g_set_error_literal(&error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "The saved search is too large.");
   g_clear_object(&info); g_object_unref(file);
   if (ok) ok = g_key_file_load_from_file(ini, filename, G_KEY_FILE_NONE, &error) && restore_search(s, ini, &error);
  }
  gtk_label_set_text(GTK_LABEL(s->status), ok ? save ? "Search saved." : "Search loaded. Click Search to run it." : error ? error->message : "Unable to read the saved search.");
  g_clear_error(&error); g_key_file_unref(ini); g_free(filename);
 }
 gtk_widget_destroy(GTK_WIDGET(dialog));
}
static void saved_search(GtkWidget *widget, gpointer data)
{
 Search *s = g_object_get_data(G_OBJECT(widget), "search"); gboolean save = GPOINTER_TO_INT(data);
 GtkWidget *dialog = gtk_file_chooser_dialog_new(save ? "Save Search" : "Open Search", GTK_WINDOW(s->window),
  save ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN, "Cancel", GTK_RESPONSE_CANCEL, save ? "Save" : "Open", GTK_RESPONSE_ACCEPT, NULL);
 gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
 if (save) { gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "Search.xpsearch"); gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE); }
 GtkFileFilter *filter = gtk_file_filter_new(); gtk_file_filter_set_name(filter, "XPWm saved searches (*.xpsearch)"); gtk_file_filter_add_pattern(filter, "*.xpsearch"); gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
 g_object_set_data(G_OBJECT(dialog), "save-search", GINT_TO_POINTER(save)); g_signal_connect(dialog, "response", G_CALLBACK(saved_response), NULL);
 xp_style_apply(dialog); gtk_widget_show(dialog);
}
static void destroyed(GtkWidget *widget, gpointer data)
{
 Search *s = data; s->dead = TRUE; stop_search(NULL, s);
 if (search_window == widget) search_window = NULL;
}
static void free_search(gpointer data)
{
 Search *s = data; g_clear_object(&s->cancel); g_object_unref(s->model); g_free(s);
}
static gboolean key(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
 if (event->keyval != GDK_KEY_Escape) return FALSE;
 Search *s = data;
 if (s->cancel) stop_search(NULL, s); else gtk_widget_destroy(widget);
 return TRUE;
}
void search_dialog_open(void)
{
 if (search_window) { gtk_window_present(GTK_WINDOW(search_window)); return; }
 Search *s = g_new0(Search, 1);
 s->window = search_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
 gtk_window_set_title(GTK_WINDOW(s->window), "Search Results");
 gtk_window_set_default_size(GTK_WINDOW(s->window), 780, 510);
 g_object_set_data_full(G_OBJECT(s->window), "search", s, free_search);
 g_signal_connect(s->window, "destroy", G_CALLBACK(destroyed), s);
 g_signal_connect(s->window, "key-press-event", G_CALLBACK(key), s);
 GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
 gtk_container_add(GTK_CONTAINER(s->window), box);
 GtkWidget *body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
 gtk_box_pack_start(GTK_BOX(box), body, TRUE, TRUE, 0);
 GtkWidget *pane = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
 gtk_container_set_border_width(GTK_CONTAINER(pane), 12);
 gtk_widget_set_name(pane, "xpwm-search-pane");
 gtk_widget_set_size_request(pane, 235, -1);
 GtkWidget *pane_scroll = gtk_scrolled_window_new(NULL, NULL);
 gtk_widget_set_hexpand(pane_scroll, FALSE);
 gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pane_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
 gtk_widget_set_size_request(pane_scroll, 260, -1); gtk_container_add(GTK_CONTAINER(pane_scroll), pane);
 gtk_box_pack_start(GTK_BOX(body), pane_scroll, FALSE, FALSE, 0);
 GtkWidget *label = gtk_label_new("Search for files and folders");
 gtk_widget_set_halign(label, GTK_ALIGN_START); gtk_box_pack_start(GTK_BOX(pane), label, FALSE, FALSE, 0);
 label = gtk_label_new_with_mnemonic("All or part of the file _name:");
 gtk_widget_set_halign(label, GTK_ALIGN_START); gtk_box_pack_start(GTK_BOX(pane), label, FALSE, FALSE, 0);
 s->name = gtk_entry_new(); gtk_label_set_mnemonic_widget(GTK_LABEL(label), s->name);
 gtk_box_pack_start(GTK_BOX(pane), s->name, FALSE, FALSE, 0);
 label = gtk_label_new_with_mnemonic("A word or phrase _in the file:");
 gtk_widget_set_halign(label, GTK_ALIGN_START); gtk_box_pack_start(GTK_BOX(pane), label, FALSE, FALSE, 0);
 s->content = gtk_entry_new(); gtk_label_set_mnemonic_widget(GTK_LABEL(label), s->content);
 gtk_widget_set_tooltip_text(s->content, "Case-insensitive UTF-8 text search; regular files up to 16 MiB. Binary files and symlinks are skipped.");
 gtk_box_pack_start(GTK_BOX(pane), s->content, FALSE, FALSE, 0);
 label = gtk_label_new_with_mnemonic("_Look in:");
 gtk_widget_set_halign(label, GTK_ALIGN_START); gtk_box_pack_start(GTK_BOX(pane), label, FALSE, FALSE, 0);
 s->folder = gtk_file_chooser_button_new("Look in", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
 gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(s->folder), g_get_home_dir());
 gtk_label_set_mnemonic_widget(GTK_LABEL(label), s->folder);
 gtk_box_pack_start(GTK_BOX(pane), s->folder, FALSE, FALSE, 0);
 s->hidden = gtk_check_button_new_with_mnemonic("Search _hidden files and folders");
 gtk_box_pack_start(GTK_BOX(pane), s->hidden, FALSE, FALSE, 0);
 s->recursive = gtk_check_button_new_with_mnemonic("Search sub_folders");
 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->recursive), TRUE);
 gtk_box_pack_start(GTK_BOX(pane), s->recursive, FALSE, FALSE, 0);
 GtkWidget *advanced = gtk_expander_new_with_mnemonic("More search _options");
 GtkWidget *grid = gtk_grid_new(); gtk_grid_set_row_spacing(GTK_GRID(grid), 6); gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
 gtk_container_add(GTK_CONTAINER(advanced), grid);
 g_object_set_data(G_OBJECT(s->window), "advanced-options", advanced);
 s->min_size = gtk_spin_button_new_with_range(0, 1073741824, 1); s->max_size = gtk_spin_button_new_with_range(0, 1073741824, 1);
 s->after = gtk_entry_new(); s->before = gtk_entry_new();
 GtkWidget *fields[] = {s->min_size, s->max_size, s->after, s->before};
 const char *labels[] = {"Minimum KiB:", "Maximum KiB:", "Modified from:", "Through:"};
 for (guint i = 0; i < G_N_ELEMENTS(fields); i++) {
  GtkWidget *caption = gtk_label_new(labels[i]); gtk_widget_set_halign(caption, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid), caption, 0, i, 1, 1); gtk_grid_attach(GTK_GRID(grid), fields[i], 1, i, 1, 1);
  gtk_widget_set_hexpand(fields[i], TRUE);
 }
 gtk_entry_set_placeholder_text(GTK_ENTRY(s->after), "YYYY-MM-DD"); gtk_entry_set_placeholder_text(GTK_ENTRY(s->before), "YYYY-MM-DD");
 gtk_widget_set_tooltip_text(s->max_size, "0 means no maximum. Size filters apply to regular files.");
 gtk_box_pack_start(GTK_BOX(pane), advanced, FALSE, FALSE, 0);
 GtkWidget *saved = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
 GtkWidget *save = gtk_button_new_with_label("Save Search…"), *load = gtk_button_new_with_label("Open Search…");
 g_object_set_data(G_OBJECT(save), "search", s); g_object_set_data(G_OBJECT(load), "search", s);
 g_signal_connect(save, "clicked", G_CALLBACK(saved_search), GINT_TO_POINTER(1)); g_signal_connect(load, "clicked", G_CALLBACK(saved_search), NULL);
 gtk_box_pack_start(GTK_BOX(saved), save, TRUE, TRUE, 0); gtk_box_pack_start(GTK_BOX(saved), load, TRUE, TRUE, 0);
 gtk_box_pack_start(GTK_BOX(pane), saved, FALSE, FALSE, 0);
 s->start = gtk_button_new_with_mnemonic("_Search"); s->stop = gtk_button_new_with_mnemonic("S_top");
 gtk_widget_set_sensitive(s->stop, FALSE);
 gtk_box_pack_start(GTK_BOX(pane), s->start, FALSE, FALSE, 0);
 gtk_box_pack_start(GTK_BOX(pane), s->stop, FALSE, FALSE, 0);
 g_signal_connect(s->start, "clicked", G_CALLBACK(start_search), s);
 g_signal_connect(s->name, "activate", G_CALLBACK(start_search), s);
 g_signal_connect(s->stop, "clicked", G_CALLBACK(stop_search), s);
 s->model = gtk_list_store_new(COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
 s->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(s->model));
 const char *titles[] = {"Name", "In Folder", "Size"};
 for (int i = 0; i < 3; i++) {
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(titles[i], renderer, "text", i, NULL);
  gtk_tree_view_column_set_resizable(column, TRUE); gtk_tree_view_column_set_min_width(column, 100);
  gtk_tree_view_column_set_expand(column, i != SIZE);
  if (i != SIZE) gtk_tree_view_column_set_sort_column_id(column, i);
  gtk_tree_view_append_column(GTK_TREE_VIEW(s->view), column);
 }
 g_signal_connect(s->view, "row-activated", G_CALLBACK(open_hit), s);
 GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
 gtk_container_add(GTK_CONTAINER(scroll), s->view); gtk_box_pack_start(GTK_BOX(body), scroll, TRUE, TRUE, 0);
 s->status = gtk_label_new("Enter a name, or use * and ? wildcards, then click Search.");
 gtk_label_set_xalign(GTK_LABEL(s->status), 0); gtk_widget_set_margin_start(s->status, 8);
 gtk_widget_set_margin_end(s->status, 8); gtk_widget_set_margin_top(s->status, 6); gtk_widget_set_margin_bottom(s->status, 6);
 gtk_label_set_ellipsize(GTK_LABEL(s->status), PANGO_ELLIPSIZE_END);
 gtk_box_pack_start(GTK_BOX(box), s->status, FALSE, FALSE, 0);
 xp_style_apply(s->window);
 gtk_style_context_add_class(gtk_widget_get_style_context(s->window), "xp-search");
 GtkCssProvider *css = gtk_css_provider_new();
 gtk_css_provider_load_from_data(css,
  "#xpwm-search-pane { background:linear-gradient(to bottom,#7ba2e7,#6375d6); }"
  ".xp-search treeview { background:#fff; color:#000; }"
  ".xp-search treeview:selected { background:#316ac5; color:#fff; }"
  ".xp-search treeview header button { background:#ece9d8; color:#000; border:1px solid #d4d0c8; min-width:0; padding:2px 4px; }", -1, NULL);
 gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(s->window), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER+3);
 g_object_unref(css);
 gtk_widget_show_all(s->window); gtk_widget_grab_focus(s->name);
}
