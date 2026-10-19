#define _XOPEN_SOURCE 700
#include "../shell/desktop-files.h"
#include <glib/gstdio.h>
#include <unistd.h>
#include <string.h>
static GFile *child(GFile *folder, const char *name) { return g_file_get_child(folder, name); }
static void content(GFile *file, const char *text)
{ GError *error = NULL; g_assert_true(g_file_replace_contents(file, text, strlen(text), NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, &error)); g_assert_no_error(error); }
static void assert_content(GFile *file, const char *expected)
{ char *text = NULL; g_assert_true(g_file_load_contents(file, NULL, &text, NULL, NULL, NULL)); g_assert_cmpstr(text, ==, expected); g_free(text); }
static void remove_tree(GFile *file)
{
 if (g_file_query_file_type(file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL) == G_FILE_TYPE_DIRECTORY) {
  GFileEnumerator *e = g_file_enumerate_children(file, G_FILE_ATTRIBUTE_STANDARD_NAME, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL); GFileInfo *info;
  while (e && (info = g_file_enumerator_next_file(e, NULL, NULL))) { GFile *entry = child(file, g_file_info_get_name(info)); remove_tree(entry); g_object_unref(entry); g_object_unref(info); }
  g_clear_object(&e);
 }
 g_file_delete(file, NULL, NULL);
}
static DesktopJob *single(DesktopOperation op, GFile *source, GFile *destination)
{ DesktopJob *job = desktop_job_new(op, destination); if (source) g_ptr_array_add(job->sources, g_object_ref(source)); return job; }
int main(void)
{
 char *path = g_dir_make_tmp("xpwm-files-test-XXXXXX", NULL), *data = g_build_filename(path, "data", NULL);
 g_setenv("XDG_DATA_HOME", data, TRUE);
 GFile *root = g_file_new_for_path(path), *src = child(root, "source"), *dst = child(root, "destination");
 g_assert_true(g_file_make_directory(src, NULL, NULL)); g_assert_true(g_file_make_directory(dst, NULL, NULL));
 GFile *a = child(src, "Report One.txt"), *b = child(src, "Other.txt"), *hidden = child(src, ".hidden");
 content(a, "original"); content(b, "existing"); content(hidden, "hidden");
 GError *error = NULL; GPtrArray *list = desktop_files_list(src, NULL, &error); g_assert_no_error(error); g_assert_cmpuint(list->len, ==, 2); g_ptr_array_unref(list);
 DesktopJob *job = single(DESKTOP_RENAME, a, NULL); job->name = g_strdup("Other.txt");
 g_assert_false(desktop_job_execute(job, NULL, &error)); g_assert_error(error, G_IO_ERROR, G_IO_ERROR_EXISTS); g_clear_error(&error); desktop_job_free(job);
 assert_content(a, "original"); assert_content(b, "existing");
 job = single(DESKTOP_RENAME, a, NULL); job->name = g_strdup("../escape"); g_assert_false(desktop_job_execute(job, NULL, &error)); g_clear_error(&error); desktop_job_free(job);
 job = single(DESKTOP_COPY, a, dst); g_assert_true(desktop_job_execute(job, NULL, &error)); g_assert_no_error(error); desktop_job_free(job);
 job = single(DESKTOP_COPY, a, dst); g_assert_true(desktop_job_execute(job, NULL, &error)); g_assert_no_error(error); desktop_job_free(job);
 GFile *copy = child(dst, "Copy of Report One.txt"), *first = child(dst, "Report One.txt"); assert_content(copy, "original"); assert_content(first, "original");
 job = single(DESKTOP_NEW_FOLDER, NULL, dst); g_assert_true(desktop_job_execute(job, NULL, &error)); desktop_job_free(job);
 job = single(DESKTOP_NEW_FOLDER, NULL, dst); g_assert_true(desktop_job_execute(job, NULL, &error)); desktop_job_free(job);
 GFile *folder = child(dst, "New Folder (2)"); g_assert_true(g_file_query_exists(folder, NULL));
 GFile *sub = child(src, "sub"), *alias = child(root, "alias"); g_file_make_directory(sub, NULL, NULL);
 char *subpath = g_file_get_path(sub); g_assert_true(g_file_make_symbolic_link(alias, subpath, NULL, NULL)); g_free(subpath);
 job = single(DESKTOP_COPY, src, alias); g_assert_false(desktop_job_execute(job, NULL, &error)); g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT); g_clear_error(&error); desktop_job_free(job);
 GFile *loop = child(src, "loop"); char *srcpath = g_file_get_path(src); g_file_make_symbolic_link(loop, srcpath, NULL, NULL); g_free(srcpath);
 job = single(DESKTOP_COPY, src, dst); g_assert_true(desktop_job_execute(job, NULL, &error)); g_assert_no_error(error); desktop_job_free(job);
 GFile *tree = child(dst, "source"), *copied_loop = child(tree, "loop"); g_assert_cmpint(g_file_query_file_type(copied_loop, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL), ==, G_FILE_TYPE_SYMBOLIC_LINK);
 job = single(DESKTOP_MOVE, b, dst); g_assert_true(desktop_job_execute(job, NULL, &error)); g_assert_false(g_file_query_exists(b, NULL)); desktop_job_free(job);
 job = single(DESKTOP_RENAME, a, NULL); job->name = g_strdup("Renamed.txt"); g_assert_true(desktop_job_execute(job, NULL, &error)); desktop_job_free(job);
 GFile *renamed = child(src, "Renamed.txt"); assert_content(renamed, "original"); g_assert_false(g_file_query_exists(a, NULL));
 GCancellable *cancel = g_cancellable_new(); g_cancellable_cancel(cancel);
 job = single(DESKTOP_COPY, renamed, dst); g_assert_false(desktop_job_execute(job, cancel, &error)); g_assert_error(error, G_IO_ERROR, G_IO_ERROR_CANCELLED); g_clear_error(&error); desktop_job_free(job); g_object_unref(cancel);
 job = single(DESKTOP_TRASH, renamed, NULL); gboolean trashed = desktop_job_execute(job, NULL, &error);
 if (trashed) { char *trashpath = g_build_filename(data, "Trash", "files", "Renamed.txt", NULL); g_assert_true(g_file_test(trashpath, G_FILE_TEST_EXISTS)); g_free(trashpath); }
 else { g_assert_nonnull(error); assert_content(renamed, "original"); g_clear_error(&error); }
 desktop_job_free(job);
 g_print("PASS: Desktop listing, rename collisions/traversal, unique folder/copy names, recursive symlink handling, alias recursion guard, real move/rename, cancellation and recoverable trash\n");
 remove_tree(root);
 GFile *all[] = {src,dst,a,b,hidden,copy,first,folder,sub,alias,loop,tree,copied_loop,renamed,root};
 for (guint i = 0; i < G_N_ELEMENTS(all); i++) g_object_unref(all[i]);
 g_free(data); g_free(path); return 0;
}
