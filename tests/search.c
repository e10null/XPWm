#define _POSIX_C_SOURCE 200809L
#include "../shell/search-dialog.c"
#include "../shared/luna.h"
#include <unistd.h>
#include <utime.h>
static Query *filtered(const char *root, const char *pattern, const char *content, gboolean hidden, gboolean cancel, gboolean shallow, guint64 minimum, guint64 maximum, gint64 after, gint64 before)
{
 Query *q = g_new0(Query, 1);
 q->content = content ? g_utf8_casefold(content, -1) : NULL;
 q->shallow = shallow; q->min_size = minimum; q->max_size = maximum; q->after = after; q->before = before;
 q->root = g_strdup(root); q->pattern = g_utf8_casefold(pattern, -1); q->hidden = hidden;
 q->hits = g_ptr_array_new_with_free_func(free_hit);
 GCancellable *c = g_cancellable_new(); if (cancel) g_cancellable_cancel(c);
 GTask *task = g_task_new(NULL, c, NULL, NULL);
 g_task_set_check_cancellable(task, FALSE); g_task_set_task_data(task, q, NULL);
 g_task_run_in_thread_sync(task, search_worker); g_assert_true(g_task_propagate_boolean(task, NULL));
 g_object_unref(task); g_object_unref(c); return q;
}
static Query *query(const char *root, const char *pattern, gboolean hidden, gboolean cancel)
{ return filtered(root, pattern, NULL, hidden, cancel, FALSE, 0, 0, 0, 0); }
static char *root;
static gboolean ui_verified;
static int ui_phase;
static gboolean ui_check(gpointer data)
{
 Search *s = g_object_get_data(G_OBJECT(search_window), "search");
 if (s->cancel) return G_SOURCE_CONTINUE;
 if (ui_phase == 0) {
  g_assert_cmpint(gtk_tree_model_iter_n_children(GTK_TREE_MODEL(s->model), NULL), ==, 2);
  gtk_entry_set_text(GTK_ENTRY(s->after), "2026-02-30"); start_search(NULL, s); g_assert_null(s->cancel);
  g_assert_nonnull(strstr(gtk_label_get_text(GTK_LABEL(s->status)), "valid modified-date"));
  gtk_entry_set_text(GTK_ENTRY(s->after), "");
  gtk_entry_set_text(GTK_ENTRY(s->content), "two"); gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->max_size), 1);
  GKeyFile *saved = search_snapshot(s); char *text = g_key_file_to_data(saved, NULL, NULL); g_key_file_unref(saved);
  saved = g_key_file_new(); g_assert_true(g_key_file_load_from_data(saved, text, -1, G_KEY_FILE_NONE, NULL)); g_free(text);
  gtk_entry_set_text(GTK_ENTRY(s->content), "changed"); GError *error = NULL;
  g_assert_true(restore_search(s, saved, &error)); g_assert_no_error(error);
  g_assert_cmpstr(gtk_entry_get_text(GTK_ENTRY(s->content)), ==, "two");
  g_key_file_set_string(saved, "Search", "Folder", "relative");
  g_assert_false(restore_search(s, saved, &error)); g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA); g_clear_error(&error); g_key_file_unref(saved);
  gtk_expander_set_expanded(GTK_EXPANDER(g_object_get_data(G_OBJECT(s->window), "advanced-options")), TRUE);
  start_search(NULL, s); ui_phase++; return G_SOURCE_CONTINUE;
 }
 g_assert_cmpint(gtk_tree_model_iter_n_children(GTK_TREE_MODEL(s->model), NULL), ==, 1);
 g_assert_nonnull(strstr(gtk_label_get_text(GTK_LABEL(s->status)), "Search complete"));
 /* Let the expanded sidebar allocation reach the compositor before capture. */
 if (ui_phase == 1) { ui_phase++; return G_SOURCE_CONTINUE; }
 if (g_getenv("XPWM_SEARCH_SCREENSHOT")) {

  char *argv[] = {"grim", (char *)g_getenv("XPWM_SEARCH_SCREENSHOT"), NULL};
  g_assert_true(g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL));
 }
 /* Closing during an active worker must cancel without accessing dead controls. */
 start_search(NULL, s); gtk_widget_destroy(search_window);
 g_assert_null(search_window); ui_verified = TRUE;
 return G_SOURCE_REMOVE;
}
static gboolean ui_done(gpointer data) { gtk_main_quit(); return G_SOURCE_REMOVE; }
int main(int argc, char **argv)
{
 root = g_dir_make_tmp("xpwm-search-test-XXXXXX", NULL);
 char *sub = g_build_filename(root, "sub", NULL), *loop = g_build_filename(sub, "loop", NULL);
 char *one = g_build_filename(root, "Report One.TXT", NULL), *two = g_build_filename(sub, "report two.txt", NULL);
 char *hidden = g_build_filename(root, ".report.txt", NULL);
 g_mkdir(sub, 0700); g_file_set_contents(one, "one", -1, NULL); g_file_set_contents(two, "two", -1, NULL);
 g_file_set_contents(hidden, "hidden", -1, NULL); g_assert_cmpint(symlink(root, loop), ==, 0);
 Query *q = query(root, "REPORT", FALSE, FALSE); g_assert_cmpuint(q->hits->len, ==, 2); free_query(q);
 q = query(root, "*.txt", TRUE, FALSE); g_assert_cmpuint(q->hits->len, ==, 3); free_query(q);
 q = query(root, "nomatch", FALSE, FALSE); g_assert_cmpuint(q->hits->len, ==, 0); free_query(q);
 q = query(root, "report", FALSE, TRUE); g_assert_true(q->stopped); g_assert_cmpuint(q->hits->len, ==, 0); free_query(q);
 g_assert_true(matches("École.TXT", "école"));
 gint64 after, before;
 g_assert_true(parse_date("2024-02-29", FALSE, &after)); g_assert_true(parse_date("2024-02-29", TRUE, &before));
 g_assert_cmpint(before-after, ==, 86400); g_assert_false(parse_date("2025-02-29", FALSE, &after));
 struct utimbuf timestamp = {.actime=1704067200, .modtime=1704067200}; g_assert_cmpint(utime(one, &timestamp), ==, 0);
 q = filtered(root, "report", NULL, FALSE, FALSE, TRUE, 0, 0, 0, 0); g_assert_cmpuint(q->hits->len, ==, 1); free_query(q);
 q = filtered(root, "report", "TWO", FALSE, FALSE, FALSE, 0, 1024, 0, 0); g_assert_cmpuint(q->hits->len, ==, 1); free_query(q);
 q = filtered(root, "report", NULL, FALSE, FALSE, FALSE, 4, 0, 0, 0); g_assert_cmpuint(q->hits->len, ==, 0); free_query(q);
 q = filtered(root, "report", NULL, FALSE, FALSE, FALSE, 0, 0, 1704067200, 1704067201); g_assert_cmpuint(q->hits->len, ==, 1); free_query(q);
 char *binary = g_build_filename(root, "binary.txt", NULL), *fifo = g_build_filename(root, "pipe.txt", NULL), *link = g_build_filename(root, "link.txt", NULL);
 g_file_set_contents(binary, "two\0binary", 10, NULL); g_assert_cmpint(mkfifo(fifo, 0600), ==, 0); g_assert_cmpint(symlink(two, link), ==, 0);
 q = filtered(root, "*.txt", "two", FALSE, FALSE, FALSE, 0, 0, 0, 0); g_assert_cmpuint(q->hits->len, ==, 1); g_assert_cmpuint(q->content_skipped, >=, 3); free_query(q);
 g_remove(binary); g_remove(fifo); g_remove(link); g_free(binary); g_free(fifo); g_free(link);
 g_print("PASS: Search size/date/content filters, shallow traversal, leap-date validation and binary/FIFO/symlink skipping\n");
 g_print("PASS: recursive Search matches case-insensitive names/wildcards, hidden files, cancellation, Unicode and symlink loops\n");
 if (argc == 2 && !strcmp(argv[1], "--gui")) {
  luna_init(); gtk_init(&argc, &argv); search_dialog_open(); GtkWidget *window = search_window;
  search_dialog_open(); g_assert_true(window == search_window);
  Search *s = g_object_get_data(G_OBJECT(window), "search");
  gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(s->folder), root);
  gtk_entry_set_text(GTK_ENTRY(s->name), "report");
  start_search(NULL, s); g_timeout_add(100, ui_check, NULL); g_timeout_add(2500, ui_done, NULL);
  gtk_main(); g_assert_true(ui_verified); luna_finish();
  g_print("PASS: native Search singleton, filtered result rows, saved-criteria roundtrip/validation and close-during-search lifecycle\n");
 }
 g_remove(loop); g_remove(one); g_remove(two); g_remove(hidden); g_rmdir(sub); g_rmdir(root);
 g_free(root); g_free(sub); g_free(loop); g_free(one); g_free(two); g_free(hidden); return 0;
}
