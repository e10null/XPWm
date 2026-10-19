#ifndef XPWM_DESKTOP_FILES_H
#define XPWM_DESKTOP_FILES_H
#include <gio/gio.h>
typedef struct {
 GFile *file; GIcon *icon; char *label;
 gboolean directory;
 guint64 size, modified;
} DesktopFile;
typedef enum { DESKTOP_COPY, DESKTOP_MOVE, DESKTOP_TRASH, DESKTOP_RENAME, DESKTOP_NEW_FOLDER } DesktopOperation;
typedef struct {
 DesktopOperation operation;
 GPtrArray *sources, *results;
 GFile *destination;
 char *name;
 guint completed, visited;
 gboolean create_destination;
} DesktopJob;
GPtrArray *desktop_files_list(GFile *folder, GCancellable *cancel, GError **error);
void desktop_file_free(gpointer item);
DesktopJob *desktop_job_new(DesktopOperation operation, GFile *destination);
void desktop_job_free(gpointer job);
gboolean desktop_job_execute(DesktopJob *job, GCancellable *cancel, GError **error);
#endif
