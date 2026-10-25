#ifndef XPWM_RUN_SUGGESTIONS_H
#define XPWM_RUN_SUGGESTIONS_H
#include <gio/gio.h>
typedef struct { char *command, *folded, *label, *search; int recent; } RunSuggestion;
GPtrArray *run_suggestions_scan(const char *path, char **history, GCancellable *cancel);
void run_suggestions_add_apps(GPtrArray *items,GCancellable *cancel);
/* Sorted borrowed candidates; caller owns the returned array. */
GPtrArray *run_suggestions_match(GPtrArray *items, const char *query, guint limit);
#endif
