#include "font-settings.h"
/* Session-local GTK3 module: keep the host's gtk.css from repainting Luna.
 * Loaded through GTK3_MODULES, so GTK2 applications never load a GTK3 library. */
#include <gtk/gtk.h>
#include <gmodule.h>
#include <fontconfig/fontconfig.h>
#include <pango/pangocairo.h>

static void keep_xp_ui_font(GtkSettings *settings,GParamSpec *property,gpointer data)
{
 char *font=NULL;(void)property;(void)data;
 g_object_get(settings,"gtk-font-name",&font,NULL);
 if(g_strcmp0(font,"Tahoma 8"))g_object_set(settings,"gtk-font-name","Tahoma 8",NULL);
 g_free(font);
}

G_MODULE_EXPORT void gtk_module_init(gint *argc, gchar ***argv)
{
	const char *path = g_getenv("XPWM_GTK_CSS");
	const char *icons = g_getenv("XPWM_GTK_ICON_THEME");
	GtkCssProvider *provider;
	GError *error = NULL;
	(void)argc; (void)argv;
	if (!path || !*path) return;
 /* Register the actual private XP faces before GTK creates any font layouts.
  * Invalidate maps that may have already resolved Tahoma to a fallback. */
 const char *theme=g_getenv("XPWM_THEME_DIR"),*root=g_getenv("XPWM_ASSET_ROOT");
 char *fonts=theme ? g_build_filename(theme,"fonts",NULL) : root ?
  g_build_filename(root,"assets","local","luna-blue-complete","fonts",NULL) : NULL;
 GDir *directory=fonts ? g_dir_open(fonts,0,NULL) : NULL;const char *name;
 while(directory && (name=g_dir_read_name(directory))) {
  char *file=g_build_filename(fonts,name,NULL);
  FcConfigAppFontAddFile(NULL,(const FcChar8 *)file);g_free(file);
 }
 if(directory){g_dir_close(directory);FcConfigBuildFonts(NULL);pango_font_map_changed(pango_cairo_font_map_get_default());}
 g_free(fonts);

	provider = gtk_css_provider_new();
	if (!gtk_css_provider_load_from_path(provider, path, &error)) {
		g_warning("XPWm Luna module: %s", error->message);
		g_clear_error(&error); g_object_unref(provider); return;
	}
	gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider),
		GTK_STYLE_PROVIDER_PRIORITY_USER + 1);
	/* CSS alone leaves GtkSettings (and widgets deriving their own fonts) on
	 * the host desktop's font. Keep XP metrics and the selected unsmoothed text
	 * local to this session. Use the saved font DPI consistently with compositor
	 * captions; monitor scaling still applies separately. */
	g_object_set(gtk_settings_get_default(),
		"gtk-font-name", "Tahoma 8",
  "gtk-xft-dpi", xp_font_dpi()*1024,
		"gtk-xft-antialias", 0, "gtk-xft-hinting", 1,
		"gtk-xft-hintstyle", "hintfull", "gtk-xft-rgba", "none",
		"gtk-enable-animations", FALSE,
		"gtk-decoration-layout", "",
		"gtk-button-images", TRUE, "gtk-menu-images", TRUE, NULL);
	xp_font_settings_watch(gtk_settings_get_default());
	g_signal_connect(gtk_settings_get_default(),"notify::gtk-font-name",G_CALLBACK(keep_xp_ui_font),NULL);
	if (icons && *icons) g_object_set(gtk_settings_get_default(), "gtk-icon-theme-name", icons, NULL);
	g_object_unref(provider);
}
