#define _XOPEN_SOURCE 700
#include "desktop-files.h"
#include <gio/gdesktopappinfo.h>
#include <stdlib.h>
#include <string.h>
void desktop_file_free(gpointer item)
{
 DesktopFile *file = item;
 g_object_unref(file->file); g_clear_object(&file->icon); g_free(file->label); g_free(file);
}
gboolean desktop_file_launch(GFile *file, GAppLaunchContext *context, GError **error)
{
 char *path = g_file_get_path(file);
 if (path && g_str_has_suffix(path, ".desktop") && !g_file_test(path, G_FILE_TEST_IS_DIR)) {
  GDesktopAppInfo *app = g_desktop_app_info_new_from_filename(path);
  if (app) {
   gboolean ok = g_app_info_launch(G_APP_INFO(app), NULL, context, error);
   g_object_unref(app); g_free(path); return ok;
  }
  GKeyFile *entry = g_key_file_new();
  gboolean ok = FALSE;
  if (g_key_file_load_from_file(entry, path, G_KEY_FILE_NONE, error)) {
   char *type = g_key_file_get_string(entry, "Desktop Entry", "Type", NULL);
   char *url = !g_strcmp0(type, "Link") ? g_key_file_get_string(entry, "Desktop Entry", "URL", NULL) : NULL;
   if (url && *url) ok = g_app_info_launch_default_for_uri(url, context, error);
   else g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "This desktop launcher is invalid or its application is not installed.");
   g_free(type); g_free(url);
  }
  g_key_file_unref(entry); g_free(path); return ok;
 }
 g_free(path);
 char *uri = g_file_get_uri(file);
 gboolean ok = g_app_info_launch_default_for_uri(uri, context, error);
 g_free(uri); return ok;
}
GPtrArray *desktop_files_list(GFile *folder, GCancellable *cancel, GError **error)
{
 GPtrArray *items = g_ptr_array_new_with_free_func(desktop_file_free);
 GFileEnumerator *enumerator = g_file_enumerate_children(folder,
  "standard::name,standard::display-name,standard::type,standard::is-hidden,standard::icon,standard::size,time::modified",
  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancel, error);
 if (!enumerator) { g_ptr_array_unref(items); return NULL; }
 GFileInfo *info;
 while ((info = g_file_enumerator_next_file(enumerator, cancel, error))) {
  if (!g_file_info_get_is_hidden(info)) {
   DesktopFile *item = g_new0(DesktopFile, 1);
   item->file = g_file_get_child(folder, g_file_info_get_name(info));
   item->label = g_strdup(g_file_info_get_display_name(info));
   item->directory = g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY;
   GIcon *icon = g_file_info_get_icon(info); item->icon = icon ? g_object_ref(icon) : NULL;
   if (!item->directory && g_str_has_suffix(g_file_info_get_name(info), ".desktop")) {
    char *path = g_file_get_path(item->file);
    GDesktopAppInfo *app = path ? g_desktop_app_info_new_from_filename(path) : NULL;
    if (app) {
     g_free(item->label); item->label = g_strdup(g_app_info_get_display_name(G_APP_INFO(app)));
     icon = g_app_info_get_icon(G_APP_INFO(app));
     if (icon) g_set_object(&item->icon, icon);
     g_object_unref(app);
    }
    g_free(path);
   }
   item->size = g_file_info_get_size(info); item->modified = g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
   g_ptr_array_add(items, item);
  }
  g_object_unref(info);
 }
 g_object_unref(enumerator);
 if (error && *error) { g_ptr_array_unref(items); return NULL; }
 return items;
}
DesktopJob *desktop_job_new(DesktopOperation operation, GFile *destination)
{
 DesktopJob *job = g_new0(DesktopJob, 1); job->operation = operation;
 job->sources = g_ptr_array_new_with_free_func(g_object_unref); job->results = g_ptr_array_new_with_free_func(g_object_unref);
 job->destination = destination ? g_object_ref(destination) : NULL; return job;
}
void desktop_job_free(gpointer data)
{
 DesktopJob *job = data; g_ptr_array_unref(job->sources); g_ptr_array_unref(job->results);
 g_clear_object(&job->destination); g_free(job->name); g_free(job);
}
static gboolean invalid(GError **error, const char *message)
{ g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, message); return FALSE; }
static gboolean valid_name(const char *name)
{ return name && *name && strcmp(name, ".") && strcmp(name, "..") && !strchr(name, '/') && !strchr(name, '\\'); }
static gboolean copy_tree(DesktopJob *job, GFile *source, GFile *target, guint depth, GCancellable *cancel, GError **error)
{
 if (g_cancellable_set_error_if_cancelled(cancel, error)) return FALSE;
 if (depth > 64 || ++job->visited > 100000) return invalid(error, "This folder is too large or too deeply nested to copy.");
 GFileInfo *info = g_file_query_info(source, G_FILE_ATTRIBUTE_STANDARD_TYPE, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancel, error);
 if (!info) return FALSE;
 gboolean directory = g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY; g_object_unref(info);
 if (!directory) return g_file_copy(source, target, G_FILE_COPY_NOFOLLOW_SYMLINKS, cancel, NULL, NULL, error);
 if (!g_file_make_directory(target, cancel, error)) return FALSE;
 GFileEnumerator *enumerator = g_file_enumerate_children(source, G_FILE_ATTRIBUTE_STANDARD_NAME, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancel, error);
 if (!enumerator) return FALSE;
 gboolean ok = TRUE;
 while (ok && (info = g_file_enumerator_next_file(enumerator, cancel, error))) {
  const char *name = g_file_info_get_name(info);
  GFile *child = g_file_get_child(source, name), *destination = g_file_get_child(target, name);
  ok = copy_tree(job, child, destination, depth+1, cancel, error);
  g_object_unref(child); g_object_unref(destination); g_object_unref(info);
 }
 g_object_unref(enumerator); return ok && !(error && *error);
}
/* Resolve the existing directory paths too: aliases through directory symlinks
 * must not turn a copy into recursion back into its own source. */
static gboolean inside_source(GFile *source, GFile *destination)
{
 char *src = g_file_get_path(source), *dst = g_file_get_path(destination);
 char *real_src = src ? realpath(src, NULL) : NULL, *real_dst = dst ? realpath(dst, NULL) : NULL;
 gboolean inside = FALSE;
 if (real_src && real_dst) {
  gsize length = strlen(real_src);
  inside = !strcmp(real_src, real_dst) || (g_str_has_prefix(real_dst, real_src) && (real_src[length-1] == '/' || real_dst[length] == '/'));
 }
 g_free(src); g_free(dst); free(real_src); free(real_dst); return inside;
}
gboolean desktop_job_execute(DesktopJob *job, GCancellable *cancel, GError **error)
{
 if (g_cancellable_set_error_if_cancelled(cancel, error)) return FALSE;
 if (job->destination && !g_file_is_native(job->destination)) return invalid(error, "Choose a local destination folder.");
 if (job->operation == DESKTOP_NEW_FOLDER) {
  for (guint n = 1; n < 10000; n++) {
   char *name = n == 1 ? g_strdup("New Folder") : g_strdup_printf("New Folder (%u)", n);
   GFile *target = g_file_get_child_for_display_name(job->destination, name, error); g_free(name);
   if (!target) return FALSE;
   GError *local = NULL;
   gboolean ok = g_file_make_directory(target, cancel, &local);
   if (ok) { g_ptr_array_add(job->results, target); job->completed++; return TRUE; }
   g_object_unref(target);
   if (!g_error_matches(local, G_IO_ERROR, G_IO_ERROR_EXISTS)) { g_propagate_error(error, local); return FALSE; }
   g_error_free(local);
  }
  return invalid(error, "Unable to find an unused folder name.");
 }
 if (job->operation == DESKTOP_RENAME && (!valid_name(job->name) || job->sources->len != 1))
  return invalid(error, "Enter a file name without slashes; the name cannot be empty, '.' or '..'.");
 for (guint i = 0; i < job->sources->len; i++) {
  GFile *source = g_ptr_array_index(job->sources, i);
  if (!g_file_is_native(source)) return invalid(error, "This operation requires local files.");
  if (g_cancellable_set_error_if_cancelled(cancel, error)) return FALSE;
  if (job->operation == DESKTOP_TRASH) {
   if (!g_file_trash(source, cancel, error)) return FALSE;
  } else {
   GFile *parent = g_file_get_parent(source), *target = NULL;
   if (!parent) return invalid(error, "The filesystem root cannot be changed.");
   if (job->operation == DESKTOP_RENAME) target = g_file_get_child_for_display_name(parent, job->name, error);
   else {
    if (g_file_equal(parent, job->destination) && job->operation == DESKTOP_MOVE) {
     g_object_unref(parent); job->completed++; continue;
    }
    if (g_file_query_file_type(source, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancel) == G_FILE_TYPE_DIRECTORY && inside_source(source, job->destination)) {
     g_object_unref(parent); return invalid(error, "A folder cannot be copied or moved inside itself.");
    }
    char *base = g_file_get_basename(source);
    for (guint n = 0; n < 10000 && !target; n++) {
     char *name = n == 0 ? g_strdup(base) : n == 1 ? g_strdup_printf("Copy of %s", base) : g_strdup_printf("Copy (%u) of %s", n, base);
     GFile *candidate = g_file_get_child(job->destination, name); g_free(name);
     if (!g_file_query_exists(candidate, cancel)) target = candidate; else g_object_unref(candidate);
    }
    g_free(base);
   }
   g_object_unref(parent);
   if (!target) return error && *error ? FALSE : invalid(error, "Unable to choose a destination name.");
   gboolean ok;
   if (g_file_equal(source, target)) ok = TRUE;
   else if (job->operation == DESKTOP_COPY) ok = copy_tree(job, source, target, 0, cancel, error);
   else ok = g_file_move(source, target, G_FILE_COPY_NOFOLLOW_SYMLINKS, cancel, NULL, NULL, error);
   if (!ok) { g_object_unref(target); return FALSE; }
   g_ptr_array_add(job->results, target);
  }
  job->completed++;
 }
 return TRUE;
}
