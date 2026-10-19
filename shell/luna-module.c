/* Session-local GTK3 module: keep the host's gtk.css from repainting Luna.
 * Loaded through GTK3_MODULES, so GTK2 applications never load a GTK3 library. */
#include <gtk/gtk.h>
#include <gmodule.h>
G_MODULE_EXPORT void gtk_module_init(gint *argc, gchar ***argv)
{
	const char *path = g_getenv("XPWM_GTK_CSS");
	const char *icons = g_getenv("XPWM_GTK_ICON_THEME");
	GtkCssProvider *provider;
	GError *error = NULL;
	(void)argc; (void)argv;
	if (!path || !*path) return;
	provider = gtk_css_provider_new();
	if (!gtk_css_provider_load_from_path(provider, path, &error)) {
		g_warning("XPWm Luna module: %s", error->message);
		g_clear_error(&error); g_object_unref(provider); return;
	}
	gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider),
		GTK_STYLE_PROVIDER_PRIORITY_USER + 1);
	/* CSS alone leaves GtkSettings (and widgets deriving their own fonts) on
	 * the host desktop's font. Keep XP metrics and standard grayscale smoothing
	 * local to processes in this session; preserve the user's display scaling. */
	g_object_set(gtk_settings_get_default(),
		"gtk-font-name", "Tahoma 8",
		"gtk-xft-antialias", 1, "gtk-xft-hinting", 1,
		"gtk-xft-hintstyle", "hintfull", "gtk-xft-rgba", "none",
		"gtk-enable-animations", FALSE,
		"gtk-decoration-layout", "",
		"gtk-button-images", TRUE, "gtk-menu-images", TRUE, NULL);
	if (icons && *icons) g_object_set(gtk_settings_get_default(), "gtk-icon-theme-name", icons, NULL);
	g_object_unref(provider);
}
