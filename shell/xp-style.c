/* Native GTK interaction, original Luna button pixels and XP control metrics. */
#include "xp-style.h"
#include "../shared/luna.h"

static gboolean button_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	GtkStateFlags flags = gtk_widget_get_state_flags(widget);
	int state = !gtk_widget_is_sensitive(widget) ? 3 :
		(flags & GTK_STATE_FLAG_ACTIVE) ? 2 :
		(flags & GTK_STATE_FLAG_PRELIGHT) ? 1 :
		gtk_widget_has_default(widget) ? 4 : 0;
	int w = gtk_widget_get_allocated_width(widget), h = gtk_widget_get_allocated_height(widget);
	if (!luna_draw(cr, "push-button", state, 0, 0, w, h)) {
		cairo_pattern_t *p = cairo_pattern_create_linear(0, 0, 0, h);
		cairo_pattern_add_color_stop_rgb(p, 0, .999, .999, .99);
		cairo_pattern_add_color_stop_rgb(p, 1, .86, .85, .79);
		cairo_set_source(cr, p); cairo_rectangle(cr, .5, .5, w-1, h-1); cairo_fill_preserve(cr);
		cairo_pattern_destroy(p); cairo_set_source_rgb(cr, .0, .24, .45);
		cairo_set_line_width(cr, 1); cairo_stroke(cr);
	}
	return FALSE; /* GTK draws the label, mnemonic and focus indication. */
}

static void style_tree(GtkWidget *widget, gpointer data)
{
	if (GTK_IS_BUTTON(widget) && !GTK_IS_CHECK_BUTTON(widget) &&
			!gtk_style_context_has_class(gtk_widget_get_style_context(widget), "xp-icon-button") &&
			!g_object_get_data(G_OBJECT(widget), "xp-button-draw")) {
		g_signal_connect(widget, "draw", G_CALLBACK(button_draw), NULL);
		g_object_set_data(G_OBJECT(widget), "xp-button-draw", GINT_TO_POINTER(1));
	}
	if (GTK_IS_CONTAINER(widget)) gtk_container_foreach(GTK_CONTAINER(widget), style_tree, NULL);
}

void xp_style_apply(GtkWidget *root)
{
 g_object_set(gtk_widget_get_settings(root),"gtk-font-name","Tahoma 8","gtk-xft-dpi",96*1024,NULL);
	static GtkCssProvider *provider;
	if (!provider) {
		provider = gtk_css_provider_new();
		gtk_css_provider_load_from_data(provider,
			".xp-shell, .xp-shell * { font-family:Tahoma; font-size:8pt; text-shadow:none; box-shadow:none; -gtk-icon-shadow:none; }"
			".xp-shell { background-color:#ece9d8; color:#000; }"
			".xp-shell label { color:inherit; }"
			".xp-shell button { background:transparent; border:0; border-radius:0; min-height:23px; min-width:75px; padding:0; color:#000; outline-offset:-4px; }"
			".xp-shell scrollbar button { min-width:17px; min-height:17px; padding:0; }"
			".xp-shell spinbutton button { min-width:16px; min-height:20px; padding:0; }"
			".xp-shell button:disabled { color:#aca899; }"
			".xp-shell button:focus { outline:1px dotted #000; }"
			".xp-shell entry { background:#fff; color:#000; border:1px solid #7f9db9; border-radius:0; min-height:20px; padding:0 3px; caret-color:#000; }"
			".xp-shell entry selection { background:#316ac5; color:#fff; }"
			".xp-shell combobox button { background:#fff; border:1px solid #7f9db9; min-width:20px; min-height:20px; padding:0 4px; }"
			".xp-shell notebook { padding:0; }"
			".xp-shell notebook > header { background:transparent; border:0; padding:0; }"
			".xp-shell notebook > header tabs { background:transparent; padding:0; }"
			".xp-shell notebook > header tab { background:#ece9d8; border:1px solid #919b9c; border-bottom:0; border-radius:3px 3px 0 0; padding:4px 7px; margin:2px 0 0; }"
			".xp-shell notebook > header tab:checked { background:#fcfcfe; border-top:3px solid #ffc73c; padding-top:3px; margin-top:0; }"
			".xp-shell notebook > stack { background:#fcfcfe; border:1px solid #919b9c; padding:12px; }"
			"menu.xp-menu { background:#fff; color:#000; border:1px solid #aca899; padding:2px; }"
			"menu.xp-menu, menu.xp-menu * { font:8pt Tahoma; text-shadow:none; box-shadow:none; }"
			"menu.xp-menu menuitem { min-height:19px; padding:0 18px 0 22px; border:0; background:transparent; color:#000; }"
			"menu.xp-menu menuitem:hover { background:#316ac5; color:#fff; }"
			"menu.xp-menu menuitem:disabled { color:#aca899; }"
			"menu.xp-menu.xp-combo-menu menuitem { padding:1px 3px; min-height:18px; }"
			"menu.xp-menu separator { min-height:1px; margin:3px 1px; background:#aca899; }", -1, NULL);
		gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(root), GTK_STYLE_PROVIDER(provider),
			GTK_STYLE_PROVIDER_PRIORITY_USER + 2);
	}
	gtk_style_context_add_class(gtk_widget_get_style_context(root), GTK_IS_MENU(root) ? "xp-menu" : "xp-shell");
	style_tree(root, NULL);
}
