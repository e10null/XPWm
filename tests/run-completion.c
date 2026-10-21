/* Exercise the actual Run widgets and launch path in a private Wayland session. */
#include "../shell/run-dialog.c"
static int phase,attempts,drain,compact_height;
static gboolean check(gpointer data)
{
 (void)data;g_assert_cmpint(++attempts,<,80);
 if(scan_cancel)return G_SOURCE_CONTINUE;
 if(phase==2) {
  if(++drain<3)return G_SOURCE_CONTINUE;
  char *passed=g_build_filename(g_getenv("XDG_RUNTIME_DIR"),"passed",NULL);
  g_assert_true(g_file_set_contents(passed,"PASS: Run autocomplete UI and selected command launch\n",-1,NULL));g_free(passed);
  g_print("PASS: Run suggestions, arrow navigation, Tab completion, Escape, selected launch, saved history and cancelled discovery\n");
  gtk_main_quit();return G_SOURCE_REMOVE;
 }
 if(phase==0) {
  compact_height=gtk_widget_get_allocated_height(dialog);
  g_assert_cmpint(compact_height,<,230);
  g_assert_false(gtk_widget_get_visible(suggestion_popup));
  gtk_entry_set_text(GTK_ENTRY(entry),"xpwm-a");
  g_assert_true(gtk_widget_get_visible(suggestion_frame));
  g_assert_true(gtk_widget_get_visible(suggestion_popup));
  g_assert_cmpint(gtk_widget_get_allocated_height(dialog),==,compact_height);
  int minimum,natural;gtk_widget_get_preferred_height(dialog,&minimum,&natural);
  g_assert_cmpint(natural,<,230);
  g_assert_true(gtk_window_get_focus(GTK_WINDOW(dialog))==entry);
  gtk_entry_set_text(GTK_ENTRY(entry),"");g_assert_false(gtk_widget_get_visible(suggestion_popup));
  gtk_entry_set_text(GTK_ENTRY(entry),"xpwm-a");
  g_assert_cmpint(gtk_tree_model_iter_n_children(GTK_TREE_MODEL(suggestion_store),NULL),==,2);
  GdkEventKey down={.keyval=GDK_KEY_Down};suggestion_key(entry,&down,NULL);
  GdkEventKey tab={.keyval=GDK_KEY_Tab};g_assert_true(suggestion_key(entry,&tab,NULL));
  g_assert_cmpstr(gtk_entry_get_text(GTK_ENTRY(entry)),==,"xpwm-apricot");
  gtk_entry_set_text(GTK_ENTRY(entry),"xpwm-a");
  GdkEventKey escape={.keyval=GDK_KEY_Escape};g_assert_true(suggestion_key(entry,&escape,NULL));
  g_assert_null(dialog);run_dialog_open();phase=3;return G_SOURCE_CONTINUE;
 }
 if(phase==3) {
  gtk_entry_set_text(GTK_ENTRY(entry),"xpwm-appl");
  gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);g_assert_null(dialog);phase=1;
  return G_SOURCE_CONTINUE;
 }
 char *marker=g_build_filename(g_getenv("XDG_RUNTIME_DIR"),"launched",NULL),*value=NULL;
 if(!g_file_get_contents(marker,&value,NULL,NULL)){g_free(marker);return G_SOURCE_CONTINUE;}
 g_assert_cmpstr(value,==,"apple");g_free(marker);g_free(value);
 char **history=run_history_load();g_assert_cmpstr(history[0],==,"xpwm-apple");g_strfreev(history);
 /* Closing during discovery must not update destroyed or reopened widgets. */
 run_dialog_open();gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_CANCEL);
 run_dialog_open();gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_CANCEL);
 phase=2;return G_SOURCE_CONTINUE;
}
int main(int argc,char **argv)
{luna_init();gtk_init(&argc,&argv);run_dialog_open();g_timeout_add(100,check,NULL);gtk_main();luna_finish();return 0;}
