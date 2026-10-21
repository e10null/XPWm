/* wlsunset is an optional session child; settings never execute shell text. */
#include <signal.h>
static GSubprocess *night_process;
static char *night_path(void) { return g_build_filename(g_get_user_config_dir(), "xpwm", "night-light.ini", NULL); }
static GKeyFile *night_load(void) {
 GKeyFile *key = g_key_file_new(); char *path = night_path();
 g_key_file_load_from_file(key, path, G_KEY_FILE_NONE, NULL); g_free(path); return key;
}
static int night_number(GKeyFile *key, const char *name, int fallback, int low, int high) {
 GError *error = NULL; int value = g_key_file_get_integer(key, "Night Light", name, &error);
 if (error) { g_error_free(error); return fallback; } return CLAMP(value, low, high);
}
static void night_stop(void) {
 if (!night_process) return;
 g_subprocess_send_signal(night_process, SIGTERM);
 g_subprocess_wait(night_process, NULL, NULL); g_clear_object(&night_process);
}
static void night_start(void) {
 GKeyFile *key = night_load();
 if (g_key_file_get_boolean(key, "Night Light", "Enabled", NULL)) {
  char rise[6], set[6], temperature[8];
  g_snprintf(rise, sizeof rise, "%02d:00", night_number(key,"Sunrise",7,0,23));
  g_snprintf(set, sizeof set, "%02d:00", night_number(key,"Sunset",19,0,23));
  g_snprintf(temperature, sizeof temperature, "%d", night_number(key,"Temperature",4000,2000,6400));
  GError *error = NULL;
  night_process = g_subprocess_new(G_SUBPROCESS_FLAGS_NONE, &error, "wlsunset", "-S", rise, "-s", set, "-t", temperature, "-T", "6500", "-d", "1800", NULL);
  if (error) { g_warning("Night Light: %s",error->message); g_error_free(error); }
 }
 g_key_file_unref(key);
}
static void night_changed(GtkWidget *widget, gpointer dialog) {
 gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_APPLY, TRUE);
}
static GtkWidget *night_page(GtkWidget *dialog) {
 GKeyFile *key = night_load(); GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
 gtk_container_set_border_width(GTK_CONTAINER(page),12);
 GtkWidget *enabled = gtk_check_button_new_with_label("Enable Night Light (wlsunset)");
 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enabled),g_key_file_get_boolean(key,"Night Light","Enabled",NULL));
 gtk_box_pack_start(GTK_BOX(page),enabled,FALSE,FALSE,0);
 g_object_set_data(G_OBJECT(page),"Enabled",enabled);
 const char *names[] = {"Sunrise", "Sunset", "Temperature"};
 const char *labels[] = {"Sunrise hour (0–23):", "Sunset hour (0–23):", "Night temperature (K):"};
 int defaults[] = {7,19,4000};
 for (int i=0;i<3;i++) {
  GtkWidget *row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8), *label=gtk_label_new(labels[i]);
  GtkWidget *spin=gtk_spin_button_new_with_range(i==2?2000:0,i==2?6400:23,i==2?100:1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin),night_number(key,names[i],defaults[i],i==2?2000:0,i==2?6400:23));
  gtk_box_pack_start(GTK_BOX(row),label,TRUE,TRUE,0); gtk_box_pack_start(GTK_BOX(row),spin,FALSE,FALSE,0);
  gtk_box_pack_start(GTK_BOX(page),row,FALSE,FALSE,0); g_object_set_data(G_OBJECT(page),names[i],spin);
  g_signal_connect(spin,"value-changed",G_CALLBACK(night_changed),dialog);
 }
 gtk_box_pack_start(GTK_BOX(page),gtk_label_new("Local time. Changes fade over 30 minutes."),FALSE,FALSE,0);
 char *program=g_find_program_in_path("wlsunset");
 if (!program) gtk_box_pack_start(GTK_BOX(page),gtk_label_new("Install wlsunset to use Night Light."),FALSE,FALSE,0);
 g_free(program); g_signal_connect(enabled,"toggled",G_CALLBACK(night_changed),dialog);
 g_key_file_unref(key); return page;
}
static gboolean night_dirty(GtkWidget *page) {
 GKeyFile *key=night_load();
 gboolean dirty=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(page),"Enabled"))) != g_key_file_get_boolean(key,"Night Light","Enabled",NULL);
 const char *names[]={"Sunrise","Sunset","Temperature"}; int defaults[]={7,19,4000};
 for(int i=0;i<3;i++) dirty |= gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(page),names[i]))) != night_number(key,names[i],defaults[i],i==2?2000:0,i==2?6400:23);
 g_key_file_unref(key); return dirty;
}
static gboolean night_apply(GtkWidget *page, GError **error) {
 GKeyFile *key=g_key_file_new();
 gboolean enabled=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(page),"Enabled")));
 char *program=g_find_program_in_path("wlsunset");
 if(enabled && !program) { g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_NOT_FOUND,"Install wlsunset first."); g_key_file_unref(key); return FALSE; }
 g_free(program);
 g_key_file_set_boolean(key,"Night Light","Enabled",enabled);
 const char *names[]={"Sunrise","Sunset","Temperature"};
 for(int i=0;i<3;i++) g_key_file_set_integer(key,"Night Light",names[i],gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(page),names[i]))));
 if(g_key_file_get_integer(key,"Night Light","Sunrise",NULL)==g_key_file_get_integer(key,"Night Light","Sunset",NULL)) {
  g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_ARGUMENT,"Sunrise and sunset must have different hours."); g_key_file_unref(key); return FALSE;
 }
 char *path=night_path(), *directory=g_path_get_dirname(path), *text=g_key_file_to_data(key,NULL,NULL);
 g_mkdir_with_parents(directory,0700); gboolean ok=g_file_set_contents(path,text,-1,error);
 g_free(path); g_free(directory); g_free(text); g_key_file_unref(key);
 if(ok) { night_stop(); night_start(); } return ok;
}
