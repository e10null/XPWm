/* Deliberately oversized GTK hints and a fixed transient, unlike foot. */
#include <gtk/gtk.h>
int main(int argc, char **argv)
{
 GtkWidget *window, *dialog;
 gtk_init(&argc, &argv);
 window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
 gtk_window_set_title(GTK_WINDOW(window), "XPWm layout hints");
 gtk_widget_set_size_request(window, 900, 650);
 gtk_container_add(GTK_CONTAINER(window), gtk_label_new("Oversized GTK minimum hints"));
 gtk_widget_show_all(window);
 dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
 gtk_window_set_title(GTK_WINDOW(dialog), "XPWm layout dialog");
 gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
 gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
 gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 160);
 gtk_container_add(GTK_CONTAINER(dialog), gtk_label_new("Fixed transient stays floating"));
 gtk_widget_show_all(dialog);
 gtk_main(); return 0;
}
