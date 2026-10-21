/* Real, unscoped GTK widgets: verifies the app theme without shell CSS providers. */
#include <gtk/gtk.h>
#include <fontconfig/fontconfig.h>
static GtkWidget *window;

static gboolean ready(gpointer data)
{
	const char *file = g_getenv("XPWM_THEME_SCREENSHOT");
	if (file) {
		char *quoted = g_shell_quote(file), *cmd = g_strdup_printf("grim %s", quoted);
		g_assert(g_spawn_command_line_sync(cmd, NULL, NULL, NULL, NULL));
		g_free(cmd); g_free(quoted);
	}
	if (g_getenv("XPWM_THEME_TEST")) {
		GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(data));
		GdkRGBA color;
		PangoFontDescription *font = NULL;
		char *font_name = NULL, *rgba = NULL;
  gint dpi=0;
		g_object_get(gtk_settings_get_default(), "gtk-font-name", &font_name,
			"gtk-xft-rgba", &rgba,"gtk-xft-dpi",&dpi, NULL);
  g_assert_cmpint(dpi,==,96*1024);
		g_assert_cmpstr(font_name, ==, "Tahoma 8");
  g_object_set(gtk_settings_get_default(),"gtk-font-name","Sans 19",NULL);
  char *restored=NULL;g_object_get(gtk_settings_get_default(),"gtk-font-name",&restored,NULL);
  g_assert_cmpstr(restored,==,"Tahoma 8");g_free(restored);
  const char *requested[]={"sans-serif","Tahoma","Arial"};
  const char *expected[]={"Tahoma","Tahoma","Arial"};
  for(guint i=0;i<G_N_ELEMENTS(requested);i++) {
   FcPattern *pattern=FcNameParse((const FcChar8 *)requested[i]);FcResult result;
   FcConfigSubstitute(NULL,pattern,FcMatchPattern);FcDefaultSubstitute(pattern);
   FcPattern *match=FcFontMatch(NULL,pattern,&result);FcChar8 *family=NULL,*path=NULL;
   g_assert_nonnull(match);g_assert_cmpint(FcPatternGetString(match,FC_FAMILY,0,&family),==,FcResultMatch);
   g_assert_cmpstr((const char *)family,==,expected[i]);
   g_assert_cmpint(FcPatternGetString(match,FC_FILE,0,&path),==,FcResultMatch);
   g_assert_nonnull(strstr((const char *)path,"luna-blue-complete/fonts/"));
   FcPatternDestroy(match);FcPatternDestroy(pattern);
  }
		g_assert_cmpstr(rgba, ==, "none");
		g_free(font_name); g_free(rgba);
		const char *css_path = g_getenv("XPWM_THEME_CSS");
		gtk_style_context_get_color(context, GTK_STATE_FLAG_NORMAL, &color);
		g_assert(color.red < .05 && color.green < .05 && color.blue < .05);
		G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		gtk_style_context_get_background_color(gtk_widget_get_style_context(window), GTK_STATE_FLAG_NORMAL, &color);
		G_GNUC_END_IGNORE_DEPRECATIONS
		g_assert_cmpfloat_with_epsilon(color.red, 236/255., .005);
		g_assert_cmpfloat_with_epsilon(color.green, 233/255., .005);
		g_assert_cmpfloat_with_epsilon(color.blue, 216/255., .005);
		g_assert_cmpstr(g_getenv("GTK_THEME"), ==, "XPWm-Luna");
		gtk_style_context_get(context, GTK_STATE_FLAG_NORMAL, "font", &font, NULL);
		g_assert(g_str_has_prefix(pango_font_description_get_family(font), "Tahoma"));
		g_assert_cmpint(pango_font_description_get_size(font), ==, 8 * PANGO_SCALE);
		PangoFont *resolved=pango_context_load_font(gtk_widget_get_pango_context(GTK_WIDGET(data)),font);
  g_assert_nonnull(resolved);PangoFontDescription *actual=pango_font_describe(resolved);
  g_assert_cmpstr(pango_font_description_get_family(actual),==,"Tahoma");
  pango_font_description_free(actual);g_object_unref(resolved);
  pango_font_description_free(font);
		if (g_getenv("XPWM_GTK_ICON_THEME")) {
			GtkIconInfo *info = gtk_icon_theme_lookup_icon(gtk_icon_theme_get_default(), "folder-documents", 32, 0);
			g_assert(info); g_assert(strstr(gtk_icon_info_get_filename(info), "/icons/XPWm-Luna/"));
			g_object_unref(info);
		}
		if (css_path) {
			GtkCssProvider *provider = gtk_css_provider_new();
			GError *error = NULL;
			g_assert(gtk_css_provider_load_from_path(provider, css_path, &error));
			g_assert_no_error(error); g_object_unref(provider);
		}
		g_print("PASS: application inherits XPWm-Luna and renders native GTK controls\n");
		gtk_main_quit();
	}
	return G_SOURCE_REMOVE;
}

int main(int argc, char **argv)
{
	GtkWidget *box, *menubar, *menu, *item, *notebook, *page, *row, *widget, *check, *scroll, *view;
	GtkListStore *store;
	GtkTreeIter iter;
	gtk_init(&argc, &argv);
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "Luna Theme Preview");
	gtk_window_set_default_size(GTK_WINDOW(window), 480, 470);
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8); gtk_container_add(GTK_CONTAINER(window), box);
	menubar = gtk_menu_bar_new(); menu = gtk_menu_new();
	item = gtk_menu_item_new_with_mnemonic("_File"); gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), item);
	item = gtk_menu_item_new_with_label("Close"); gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect_swapped(item, "activate", G_CALLBACK(gtk_widget_destroy), window);
	gtk_box_pack_start(GTK_BOX(box), menubar, FALSE, FALSE, 0);
	notebook = gtk_notebook_new(); gtk_widget_set_margin_start(notebook, 9); gtk_widget_set_margin_end(notebook, 9);
	gtk_box_pack_start(GTK_BOX(box), notebook, TRUE, TRUE, 0);
	page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); gtk_container_set_border_width(GTK_CONTAINER(page), 6);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page, gtk_label_new("Controls"));
	widget = gtk_label_new("Windows XP Luna Blue"); gtk_widget_set_halign(widget, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(page), widget, FALSE, FALSE, 0);
	row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6); gtk_box_pack_start(GTK_BOX(page), row, FALSE, FALSE, 0);
	for (int i = 0; i < 3; i++) {
		widget = gtk_button_new_with_label(i == 0 ? "OK" : i == 1 ? "Cancel" : "Disabled");
		gtk_widget_set_size_request(widget, 75, -1); gtk_widget_set_sensitive(widget, i != 2);
		gtk_box_pack_start(GTK_BOX(row), widget, FALSE, FALSE, 0);
		if (i == 0) { gtk_widget_set_can_default(widget, TRUE); gtk_widget_grab_default(widget); }
	}
	row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12); gtk_box_pack_start(GTK_BOX(page), row, FALSE, FALSE, 0);
	check = gtk_check_button_new_with_label("Checked"); gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), TRUE);
	gtk_box_pack_start(GTK_BOX(row), check, FALSE, FALSE, 0);
	widget = gtk_check_button_new_with_label("Unchecked"); gtk_box_pack_start(GTK_BOX(row), widget, FALSE, FALSE, 0);
	widget = gtk_radio_button_new_with_label(NULL, "Selected"); gtk_box_pack_start(GTK_BOX(row), widget, FALSE, FALSE, 0);
	widget = gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(widget), "Tahoma, white fields and Luna control borders");
	gtk_box_pack_start(GTK_BOX(page), widget, FALSE, FALSE, 0);
	widget = gtk_combo_box_text_new(); gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "Windows XP style");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "Luna Blue"); gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);
	gtk_box_pack_start(GTK_BOX(page), widget, FALSE, FALSE, 0);
	widget = gtk_progress_bar_new(); gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(widget), .63);
	gtk_box_pack_start(GTK_BOX(page), widget, FALSE, FALSE, 0);
	widget = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
	gtk_scale_set_draw_value(GTK_SCALE(widget), FALSE); gtk_range_set_value(GTK_RANGE(widget), 40);
	gtk_box_pack_start(GTK_BOX(page), widget, FALSE, FALSE, 0);
	store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	for (int i = 0; i < 24; i++) {
		char *name = g_strdup_printf("Document %02d", i+1);
		gtk_list_store_append(store, &iter); gtk_list_store_set(store, &iter, 0, name, 1, "File folder", 2, "folder", -1); g_free(name);
	}
	view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store)); g_object_unref(store);
	{
		GtkTreeViewColumn *column = gtk_tree_view_column_new();
		GtkCellRenderer *icon = gtk_cell_renderer_pixbuf_new(), *label = gtk_cell_renderer_text_new();
		gtk_tree_view_column_set_title(column, "Name");
		g_object_set(icon, "stock-size", GTK_ICON_SIZE_MENU, NULL);
		gtk_tree_view_column_pack_start(column, icon, FALSE);
		gtk_tree_view_column_add_attribute(column, icon, "icon-name", 2);
		gtk_tree_view_column_pack_start(column, label, TRUE);
		gtk_tree_view_column_add_attribute(column, label, "text", 0);
		gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
	}
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, "Type", gtk_cell_renderer_text_new(), "text", 1, NULL);
	scroll = gtk_scrolled_window_new(NULL, NULL); gtk_widget_set_size_request(scroll, -1, 100);
	gtk_container_add(GTK_CONTAINER(scroll), view); gtk_box_pack_start(GTK_BOX(page), scroll, TRUE, TRUE, 0);
	widget = gtk_label_new("Theme colors and control artwork are inherited by GTK applications.");
	gtk_label_set_line_wrap(GTK_LABEL(widget), TRUE); gtk_notebook_append_page(GTK_NOTEBOOK(notebook), widget, gtk_label_new("About"));
	widget = gtk_label_new("XPWm Luna"); gtk_widget_set_halign(widget, GTK_ALIGN_START); gtk_widget_set_margin_start(widget, 10);
	gtk_box_pack_start(GTK_BOX(box), widget, FALSE, FALSE, 5);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	gtk_widget_show_all(window); gtk_widget_grab_focus(check);
	if (g_getenv("XPWM_THEME_TEST") || g_getenv("XPWM_THEME_SCREENSHOT")) g_timeout_add(500, ready, check);
	gtk_main(); return 0;
}
