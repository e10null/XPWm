/* Standalone XP Run dialog for the compositor's launcher shortcuts. */
#include "run-dialog.h"
#include "../shared/luna.h"
int main(int argc, char **argv)
{
	GtkWidget *dialog;
	luna_init(); gtk_init(&argc, &argv);
	dialog = run_dialog_open();
	g_signal_connect(dialog, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	gtk_main(); luna_finish(); return 0;
}
