#ifndef XPWM_SCREENSAVER_CONFIG_H
#define XPWM_SCREENSAVER_CONFIG_H
#include <glib.h>
#include <errno.h>
enum { XP_SAVER_NONE, XP_SAVER_FLOWER, XP_SAVER_FLYING, XP_SAVER_PIPES,
 XP_SAVER_TEXT, XP_SAVER_BEZIERS, XP_SAVER_BLANK, XP_SAVER_MARQUEE,
 XP_SAVER_PICTURES, XP_SAVER_MYSTIFY, XP_SAVER_STARS, XP_SAVER_LOGO, XP_SAVER_GENTOO, XP_SAVER_COUNT };
static const char *const xp_saver_names[] = {"(None)", "3D FlowerBox", "3D Flying Objects", "3D Pipes", "3D Text", "Beziers", "Blank", "Marquee", "My Pictures Slideshow", "Mystify", "Starfield", "Windows XP", "Gentoo ASCII DVD"};
typedef struct { int mode, minutes; char *text, *pictures; gboolean lock; int lock_minutes; } XpSaverConfig;
static inline char *xp_saver_config_path(void)
{ return g_build_filename(g_get_user_config_dir(), "xpwm", "screensaver.ini", NULL); }
static inline void xp_saver_config_clear(XpSaverConfig *c)
{ g_clear_pointer(&c->text, g_free); g_clear_pointer(&c->pictures, g_free); }
static inline void xp_saver_config_copy(XpSaverConfig *to, const XpSaverConfig *from)
{ *to = *from; to->text = g_strdup(from->text); to->pictures = g_strdup(from->pictures); }
static inline void xp_saver_config_load(XpSaverConfig *c)
{
 const char *pictures = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
 char *path = xp_saver_config_path(); GKeyFile *ini = g_key_file_new();
 *c = (XpSaverConfig){XP_SAVER_NONE, 10, g_strdup("Windows XP"), g_strdup(pictures ? pictures : g_get_home_dir()), FALSE, 15};
 if (g_key_file_load_from_file(ini, path, G_KEY_FILE_NONE, NULL)) {
  char *mode = g_key_file_get_string(ini, "ScreenSaver", "Name", NULL);
  int minutes = g_key_file_get_integer(ini, "ScreenSaver", "WaitMinutes", NULL);
  const char *fields[] = {"Text", "PicturesFolder"}; char **values[] = {&c->text, &c->pictures};
  c->lock = g_key_file_get_boolean(ini, "ScreenSaver", "Lock", NULL);
  for (int i = 0; i < XP_SAVER_COUNT; i++) if (!g_strcmp0(mode, xp_saver_names[i])) c->mode = i;
  g_free(mode);
  if (minutes >= 1 && minutes <= 120) c->minutes = minutes;
  minutes = g_key_file_get_integer(ini, "ScreenSaver", "LockMinutes", NULL);
  if (minutes >= 1 && minutes <= 120) c->lock_minutes = minutes;
  for (int i = 0; i < 2; i++) {
   char *value = g_key_file_get_string(ini, "ScreenSaver", fields[i], NULL);
   if (value && g_utf8_validate(value, -1, NULL)) { g_free(*values[i]); *values[i] = value; }
   else g_free(value);
  }
 }
 g_free(path); g_key_file_unref(ini);
}
static inline gboolean xp_saver_config_save(const XpSaverConfig *c, GError **error)
{
 char *path = xp_saver_config_path(), *dir = g_path_get_dirname(path);
 GKeyFile *ini = g_key_file_new();
 gboolean ok;
 char *contents;
 g_key_file_set_string(ini, "ScreenSaver", "Name", xp_saver_names[CLAMP(c->mode, 0, XP_SAVER_COUNT-1)]);
 g_key_file_set_boolean(ini, "ScreenSaver", "Lock", c->lock);
 g_key_file_set_integer(ini, "ScreenSaver", "LockMinutes", CLAMP(c->lock_minutes, 1, 120));
 g_key_file_set_integer(ini, "ScreenSaver", "WaitMinutes", CLAMP(c->minutes, 1, 120));
 g_key_file_set_string(ini, "ScreenSaver", "Text", c->text ? c->text : "Windows XP");
 g_key_file_set_string(ini, "ScreenSaver", "PicturesFolder", c->pictures ? c->pictures : "");
 ok = g_mkdir_with_parents(dir, 0700) == 0;
 if (!ok) g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno), "Unable to create screen saver settings: %s", g_strerror(errno));
 contents = g_key_file_to_data(ini, NULL, NULL);
 if (ok) ok = g_file_set_contents(path, contents, -1, error);
 g_free(contents); g_free(path); g_free(dir); g_key_file_unref(ini); return ok;
}
#endif
