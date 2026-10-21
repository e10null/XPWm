/* Session-local font DPI. Display/output scaling is managed independently. */
#ifndef XPWM_FONT_SETTINGS_H
#define XPWM_FONT_SETTINGS_H
#include <glib.h>
#include <errno.h>
#include <sys/stat.h>
static inline char *xp_font_settings_path(void)
{return g_build_filename(g_get_user_config_dir(),"xpwm","appearance.ini",NULL);}
static inline int xp_font_dpi_read(void)
{
 GKeyFile *key=g_key_file_new();char *path=xp_font_settings_path();int dpi=96;
 if(g_key_file_load_from_file(key,path,G_KEY_FILE_NONE,NULL)) {
  int value=g_key_file_get_integer(key,"Appearance","FontDPI",NULL);
  if(value==96 || value==120 || value==144)dpi=value;
 }
 g_key_file_unref(key);g_free(path);return dpi;
}
static inline int xp_font_dpi(void)
{
 static char *path;static struct stat saved;static gboolean seen;static int dpi=96;
 struct stat current;
 if(!path)path=xp_font_settings_path();
 if(stat(path,&current)<0){seen=FALSE;dpi=96;}
 else if(!seen || saved.st_ino!=current.st_ino || saved.st_mtime!=current.st_mtime || saved.st_size!=current.st_size) {
  dpi=xp_font_dpi_read();saved=current;seen=TRUE;
 }
 return dpi;
}
static inline gboolean xp_font_dpi_save(int dpi,GError **error)
{
 GKeyFile *key;char *path,*directory,*contents;gboolean ok;
 if(dpi!=96 && dpi!=120 && dpi!=144){g_set_error_literal(error,G_FILE_ERROR,G_FILE_ERROR_INVAL,"Unsupported font DPI");return FALSE;}
 key=g_key_file_new();path=xp_font_settings_path();directory=g_path_get_dirname(path);
 g_key_file_load_from_file(key,path,G_KEY_FILE_NONE,NULL);g_key_file_set_integer(key,"Appearance","FontDPI",dpi);
 ok=g_mkdir_with_parents(directory,0700)==0;
 if(!ok)g_set_error(error,G_FILE_ERROR,g_file_error_from_errno(errno),"Unable to create appearance settings: %s",g_strerror(errno));
 contents=g_key_file_to_data(key,NULL,NULL);
 if(ok)ok=g_file_set_contents(path,contents,-1,error);
 g_free(contents);g_free(directory);g_free(path);g_key_file_unref(key);return ok;
}
#endif
