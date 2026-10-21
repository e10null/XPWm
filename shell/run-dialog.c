/* XP Run layout with native GTK input and original Luna controls. */
#include "run-dialog.h"
#include "run-command.h"
#include "run-history.h"
#include "run-suggestions.h"
#include "xp-style.h"
#include "../shared/luna.h"
#include "../shared/sounds.h"

static GtkWidget *dialog, *entry, *message, *chooser;
static GtkWidget *suggestion_frame,*suggestion_view,*suggestion_popup;
static GtkListStore *suggestion_store;
static GPtrArray *suggestions;
static GCancellable *scan_cancel;

static void refresh_suggestions(void)
{
 if(!suggestions || !suggestion_store)return;
 GPtrArray *matches=run_suggestions_match(suggestions,gtk_entry_get_text(GTK_ENTRY(entry)),30);
 gtk_list_store_clear(suggestion_store);
 for(guint i=0;i<matches->len;i++) {
  RunSuggestion *item=g_ptr_array_index(matches,i);GtkTreeIter iter;
  gtk_list_store_append(suggestion_store,&iter);
  gtk_list_store_set(suggestion_store,&iter,0,item->command,-1);
 }
 gboolean show=matches->len>0 && *gtk_entry_get_text(GTK_ENTRY(entry));
 gtk_widget_set_size_request(suggestion_frame,MAX(220,gtk_widget_get_allocated_width(entry)),MIN(matches->len,6)*22+4);
 gtk_widget_set_visible(suggestion_frame,show);
 if(show){gtk_widget_show(suggestion_popup);gtk_popover_popup(GTK_POPOVER(suggestion_popup));}
 else gtk_widget_hide(suggestion_popup);
 if(matches->len) {
  GtkTreePath *path=gtk_tree_path_new_first();
  gtk_tree_view_set_cursor(GTK_TREE_VIEW(suggestion_view),path,NULL,FALSE);
  gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(suggestion_view),path,NULL,FALSE,0,0);gtk_tree_path_free(path);
 }
 g_ptr_array_unref(matches);

}
static gboolean complete_selected(void)
{
 if(!suggestion_frame || !gtk_widget_get_visible(suggestion_frame))return FALSE;
 GtkTreeIter iter;GtkTreeModel *model;char *command=NULL;
 if(!gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(suggestion_view)),&model,&iter))return FALSE;
 gtk_tree_model_get(model,&iter,0,&command,-1);
 gtk_entry_set_text(GTK_ENTRY(entry),command);gtk_editable_set_position(GTK_EDITABLE(entry),-1);g_free(command);
 gtk_widget_hide(suggestion_popup);gtk_widget_hide(suggestion_frame);
 return TRUE;
}
static gboolean suggestion_focus_out(GtkWidget *widget,GdkEventFocus *event,gpointer data)
{
 (void)widget;(void)event;(void)data;
 gtk_widget_hide(suggestion_popup);gtk_widget_hide(suggestion_frame);return FALSE;
}
static gboolean suggestion_key(GtkWidget *widget,GdkEventKey *event,gpointer data)
{
 (void)widget;(void)data;
 if(event->state&(GDK_CONTROL_MASK|GDK_MOD1_MASK|GDK_SUPER_MASK))return FALSE;
 if(event->keyval==GDK_KEY_Escape) {
  gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_CANCEL);return TRUE;
 }
 if(event->keyval==GDK_KEY_Down || event->keyval==GDK_KEY_Up) {
  if(!gtk_widget_get_visible(suggestion_frame)){refresh_suggestions();return TRUE;}
  GtkTreeSelection *selection=gtk_tree_view_get_selection(GTK_TREE_VIEW(suggestion_view));
  GtkTreeModel *model=GTK_TREE_MODEL(suggestion_store);GtkTreeIter iter;
  int count=gtk_tree_model_iter_n_children(model,NULL),index=-1;
  if(!count)return TRUE;
  if(gtk_tree_selection_get_selected(selection,NULL,&iter)) {
   GtkTreePath *path=gtk_tree_model_get_path(model,&iter);index=gtk_tree_path_get_indices(path)[0];gtk_tree_path_free(path);
  }
  index=(index+(event->keyval==GDK_KEY_Down ? 1 : count-1)+count)%count;
  GtkTreePath *path=gtk_tree_path_new_from_indices(index,-1);
  gtk_tree_view_set_cursor(GTK_TREE_VIEW(suggestion_view),path,NULL,FALSE);
  gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(suggestion_view),path,NULL,FALSE,0,0);gtk_tree_path_free(path);
  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),GTK_RESPONSE_OK,TRUE);return TRUE;
 }
 if(event->keyval==GDK_KEY_Tab && !(event->state&GDK_SHIFT_MASK))return complete_selected();
 return FALSE;
}
static void suggestion_activated(GtkTreeView *view,GtkTreePath *path,GtkTreeViewColumn *column,gpointer data)
{
 (void)view;(void)path;(void)column;(void)data;
 gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);
}
typedef struct {char *path;char **history;} ScanInput;
static void scan_input_free(gpointer data)
{ScanInput *input=data;g_free(input->path);g_strfreev(input->history);g_free(input);}
static void scan_worker(GTask *task,gpointer source,gpointer data,GCancellable *cancel)
{
 ScanInput *input=data;(void)source;
 g_task_return_pointer(task,run_suggestions_scan(input->path,input->history,cancel),(GDestroyNotify)g_ptr_array_unref);
}
static void scan_ready(GObject *source,GAsyncResult *result,gpointer data)
{
 GPtrArray *items=g_task_propagate_pointer(G_TASK(result),NULL);(void)data;
 if(GTK_WIDGET(source)!=dialog){if(items)g_ptr_array_unref(items);return;}
 g_clear_object(&scan_cancel);
 if(items){g_clear_pointer(&suggestions,g_ptr_array_unref);suggestions=items;refresh_suggestions();}
}


static gboolean run_icon(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	if (!xp_icon(cr, 25, 32, 0, 4)) {
		cairo_set_source_rgb(cr, .2, .4, .8);
		cairo_rectangle(cr, 1, 7, 30, 22); cairo_fill(cr);
	}
	return FALSE;
}

static void changed(GtkEditable *editable, gpointer data)
{
	char *value = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK, *g_strstrip(value) != '\0');
	gtk_widget_hide(message);
 refresh_suggestions();
	g_free(value);
}

static void browse_response(GtkDialog *window, gint response, gpointer data)
{
	if (response == GTK_RESPONSE_ACCEPT) {
		char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(window));
		if (filename) {
			char *quoted = g_shell_quote(filename);
			gtk_entry_set_text(GTK_ENTRY(entry), quoted);
			g_free(quoted); g_free(filename);
		}
	}
	gtk_widget_destroy(GTK_WIDGET(window));
	chooser = NULL;
	gtk_widget_grab_focus(entry);
}

static void response(GtkDialog *window, gint response_id, gpointer data)
{
	GError *error = NULL;
 if(response_id==GTK_RESPONSE_OK)complete_selected();
	if (response_id == GTK_RESPONSE_APPLY) {
		if (chooser) { gtk_window_present(GTK_WINDOW(chooser)); return; }
		chooser = gtk_file_chooser_dialog_new("Browse", GTK_WINDOW(dialog), GTK_FILE_CHOOSER_ACTION_OPEN,
			"_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
		gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(chooser), TRUE);
		gtk_window_set_modal(GTK_WINDOW(chooser), TRUE);
		g_signal_connect(chooser, "response", G_CALLBACK(browse_response), NULL);
		gtk_widget_show(chooser);
		return;
	}
	if (response_id == GTK_RESPONSE_OK &&
			!run_command_launch(gtk_entry_get_text(GTK_ENTRY(entry)), &error)) {
		xp_sound_play(XPWM_SOUND_ERROR);
		gtk_label_set_text(GTK_LABEL(message), error ? error->message : "Unable to start program.");
		g_clear_error(&error);
		gtk_widget_show(message);
		gtk_widget_grab_focus(entry);
		return;
	}
	if (response_id == GTK_RESPONSE_OK) {
  const char *command = gtk_entry_get_text(GTK_ENTRY(entry));
  run_history_record(command, NULL);
  char *uri = run_command_target_uri(command);
  if (uri) {
   GFile *file = g_file_new_for_uri(uri);
   if (g_file_is_native(file) && g_file_query_file_type(file, G_FILE_QUERY_INFO_NONE, NULL) == G_FILE_TYPE_REGULAR)
    gtk_recent_manager_add_item(gtk_recent_manager_get_default(), uri);
   g_object_unref(file); g_free(uri);
  }
 }
	gtk_widget_destroy(GTK_WIDGET(window));
}

static void destroyed(GtkWidget *widget, gpointer data)
{
	if (chooser) { gtk_widget_destroy(chooser); chooser = NULL; }
	gtk_style_context_remove_provider_for_screen(gtk_widget_get_screen(widget), GTK_STYLE_PROVIDER(data));
 if(scan_cancel)g_cancellable_cancel(scan_cancel);
 g_clear_object(&scan_cancel);g_clear_pointer(&suggestions,g_ptr_array_unref);
 if(suggestion_popup)gtk_widget_destroy(suggestion_popup);
 suggestion_frame=suggestion_view=suggestion_popup=NULL;suggestion_store=NULL;
	dialog = entry = message = NULL;
}

GtkWidget *run_dialog_open(void)
{
	GtkWidget *box, *grid, *label, *intro, *icon, *actions;
	GtkCssProvider *css;
	if (dialog) { gtk_window_present(GTK_WINDOW(dialog)); return dialog; }
	dialog = gtk_dialog_new_with_buttons("Run", NULL, 0,
		"_OK", GTK_RESPONSE_OK, "_Cancel", GTK_RESPONSE_CANCEL,
		"_Browse...", GTK_RESPONSE_APPLY, NULL);
	gtk_widget_set_name(dialog, "xpwm-run");
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 399, -1);
	css = gtk_css_provider_new();
	gtk_css_provider_load_from_data(css,
		"#xpwm-run { background:#ece9d8; color:black; font:8pt Tahoma; }"
		"#xpwm-run button { min-height:21px; min-width:65px; padding:0 4px; margin:0; }"
		"#xpwm-run entry { min-height:21px; padding:0 3px; margin:0; }"
  "#xpwm-run-dropdown { border-radius:0; padding:0; background:#fff; color:#000; font:8pt Tahoma; }"
  "#xpwm-run-dropdown treeview { background:#fff; color:#000; font:8pt Tahoma; }"
  "#xpwm-run-dropdown treeview:selected { background:#316ac5; color:white; }", -1, NULL);
	gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(dialog), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER+1);
	g_object_set_data_full(G_OBJECT(dialog), "run-css", css, g_object_unref);
	g_signal_connect(dialog, "destroy", G_CALLBACK(destroyed), css);
	box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_set_border_width(GTK_CONTAINER(box), 12);
	gtk_box_set_spacing(GTK_BOX(box), 14);
	intro = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
	icon = gtk_drawing_area_new(); gtk_widget_set_size_request(icon, 32, 40);
	g_signal_connect(icon, "draw", G_CALLBACK(run_icon), NULL);
	gtk_box_pack_start(GTK_BOX(intro), icon, FALSE, FALSE, 0);
	label = gtk_label_new("Type the name of a program, folder, document, or\nInternet resource, and Windows will open it for you.");
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_box_pack_start(GTK_BOX(intro), label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), intro, FALSE, FALSE, 0);
	grid = gtk_grid_new(); gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	label = gtk_label_new_with_mnemonic("_Open:");
	GtkWidget *history = gtk_combo_box_text_new_with_entry();
 char **commands = run_history_load();
 for (guint i = 0; commands[i]; i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(history), commands[i]);
 g_strfreev(commands);
 entry = gtk_bin_get_child(GTK_BIN(history));
 g_object_set_data(G_OBJECT(dialog), "run-history-combo", history);
 gtk_widget_set_hexpand(history, TRUE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), history, 1, 0, 1, 1);
	gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 0);
 suggestion_frame=gtk_scrolled_window_new(NULL,NULL);
 gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(suggestion_frame),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
 gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(suggestion_frame),GTK_SHADOW_IN);
 gtk_widget_set_no_show_all(suggestion_frame,TRUE);
 suggestion_popup=gtk_popover_new(entry);
 gtk_popover_set_modal(GTK_POPOVER(suggestion_popup),FALSE);
 gtk_popover_set_position(GTK_POPOVER(suggestion_popup),GTK_POS_BOTTOM);
 gtk_widget_set_name(suggestion_popup,"xpwm-run-dropdown");
 gtk_container_add(GTK_CONTAINER(suggestion_popup),suggestion_frame);
 suggestion_store=gtk_list_store_new(1,G_TYPE_STRING);
 suggestion_view=gtk_tree_view_new_with_model(GTK_TREE_MODEL(suggestion_store));g_object_unref(suggestion_store);
 gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(suggestion_view),FALSE);
 gtk_tree_view_set_enable_search(GTK_TREE_VIEW(suggestion_view),FALSE);
 gtk_tree_view_set_activate_on_single_click(GTK_TREE_VIEW(suggestion_view),TRUE);
 gtk_widget_set_can_focus(suggestion_view,FALSE);
 GtkCellRenderer *renderer=gtk_cell_renderer_text_new();
 g_object_set(renderer,"ellipsize",PANGO_ELLIPSIZE_END,"ypad",3,NULL);
 gtk_tree_view_append_column(GTK_TREE_VIEW(suggestion_view),gtk_tree_view_column_new_with_attributes("Command",renderer,"text",0,NULL));
 gtk_container_add(GTK_CONTAINER(suggestion_frame),suggestion_view);gtk_widget_show(suggestion_view);

 g_object_set_data(G_OBJECT(dialog),"run-suggestion-view",suggestion_view);
 g_signal_connect(entry,"key-press-event",G_CALLBACK(suggestion_key),NULL);
 g_signal_connect(entry,"focus-out-event",G_CALLBACK(suggestion_focus_out),NULL);
 g_signal_connect(suggestion_view,"row-activated",G_CALLBACK(suggestion_activated),NULL);

	message = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(message), TRUE);
	gtk_label_set_max_width_chars(GTK_LABEL(message), 50);
	gtk_label_set_xalign(GTK_LABEL(message), 0);
	gtk_widget_set_no_show_all(message, TRUE);
	gtk_box_pack_start(GTK_BOX(box), message, FALSE, FALSE, 0);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK, FALSE);
	g_signal_connect(entry, "changed", G_CALLBACK(changed), NULL);
	g_signal_connect(dialog, "response", G_CALLBACK(response), NULL);
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	actions = gtk_dialog_get_action_area(GTK_DIALOG(dialog));
	G_GNUC_END_IGNORE_DEPRECATIONS
	gtk_container_set_border_width(GTK_CONTAINER(actions), 12);
	gtk_box_set_spacing(GTK_BOX(actions), 6);
	xp_style_apply(dialog);
	gtk_widget_show_all(dialog);
	gtk_widget_grab_focus(entry);
 /* Show history immediately; discover PATH off the UI thread. */
 char **recent=run_history_load();suggestions=run_suggestions_scan("/nonexistent/xpwm",recent,NULL);
 g_strfreev(recent);refresh_suggestions();
 scan_cancel=g_cancellable_new();ScanInput *input=g_new0(ScanInput,1);
 input->path=g_strdup(g_getenv("PATH"));input->history=run_history_load();
 GTask *task=g_task_new(dialog,scan_cancel,scan_ready,NULL);g_task_set_task_data(task,input,scan_input_free);
 g_task_run_in_thread(task,scan_worker);g_object_unref(task);
	return dialog;
}
