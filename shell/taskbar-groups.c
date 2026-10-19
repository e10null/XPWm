#include "taskbar-groups.h"
GPtrArray *taskbar_groups_build(GList *windows, const char *(*app_id)(gpointer), gboolean enabled, int available)
{
 GPtrArray *groups = g_ptr_array_new_with_free_func((GDestroyNotify)g_ptr_array_unref);
 GHashTable *counts = g_hash_table_new(g_str_hash, g_str_equal);
 GHashTable *combined = g_hash_table_new(g_str_hash, g_str_equal);
 guint buttons = g_list_length(windows);
 for (GList *l = windows; l; l = l->next) {
  const char *id = app_id(l->data);
  if (id && *id) g_hash_table_insert(counts, (gpointer)id, GUINT_TO_POINTER(GPOINTER_TO_UINT(g_hash_table_lookup(counts, id))+1));
 }
 /* Collapse the largest applications first, with stable first-window ties. */
 while (enabled && buttons > 1 && (gint64)buttons*100 > MAX(0, available)) {
  const char *best = NULL; guint largest = 1;
  for (GList *l = windows; l; l = l->next) {
   const char *id = app_id(l->data);
   if (!id || !*id || g_hash_table_contains(combined, id)) continue;
   guint count = GPOINTER_TO_UINT(g_hash_table_lookup(counts, id));
   if (count > largest) { best = id; largest = count; }
  }
  if (!best) break;
  g_hash_table_add(combined, (gpointer)best); buttons -= largest-1;
 }
 GHashTable *by_id = g_hash_table_new(g_str_hash, g_str_equal);
 for (GList *l = windows; l; l = l->next) {
  const char *id = app_id(l->data);
  gboolean combine = id && *id && g_hash_table_contains(combined, id);
  GPtrArray *group = combine ? g_hash_table_lookup(by_id, id) : NULL;
  if (!group) {
   group = g_ptr_array_new(); g_ptr_array_add(groups, group);
   if (combine) g_hash_table_insert(by_id, (gpointer)id, group);
  }
  g_ptr_array_add(group, l->data);
 }
 g_hash_table_unref(by_id); g_hash_table_unref(combined); g_hash_table_unref(counts);
 return groups;
}
