/* Draft font preference obeys the Display Properties Apply/Cancel buttons. */
#include "font-settings.h"
static void appearance_changed(GtkWidget *widget,gpointer dialog) {
 gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),GTK_RESPONSE_APPLY,TRUE);
}
static GtkWidget *appearance_page(GtkWidget *dialog) {
 GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,12),*combo=gtk_combo_box_text_new();
 gtk_container_set_border_width(GTK_CONTAINER(box),12);
 gtk_box_pack_start(GTK_BOX(box),gtk_label_new("Font size (DPI):"),FALSE,FALSE,0);
 gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo),"96","Normal — 100% (96 DPI)");
 gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo),"120","Large — 125% (120 DPI)");
 gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo),"144","Extra large — 150% (144 DPI)");
 char id[8];g_snprintf(id,sizeof id,"%d",xp_font_dpi_read());gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo),id);
 gtk_box_pack_start(GTK_BOX(box),combo,FALSE,FALSE,0);
 GtkWidget *note=gtk_label_new("Changes text size while keeping the display sharp.\nUse Settings → Display scaling to enlarge the whole interface.\nSome applications need to be reopened.");
 gtk_label_set_line_wrap(GTK_LABEL(note),TRUE);gtk_box_pack_start(GTK_BOX(box),note,FALSE,FALSE,0);
 g_object_set_data(G_OBJECT(box),"font-dpi-combo",combo);g_signal_connect(combo,"changed",G_CALLBACK(appearance_changed),dialog);
 return box;
}
static int appearance_dpi(GtkWidget *page) {
 const char *id=gtk_combo_box_get_active_id(GTK_COMBO_BOX(g_object_get_data(G_OBJECT(page),"font-dpi-combo")));
 return id ? atoi(id) : 96;
}
static gboolean appearance_dirty(GtkWidget *page) {return appearance_dpi(page)!=xp_font_dpi_read();}
