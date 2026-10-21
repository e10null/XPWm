#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include "../shell/night-light.h"
int main(void) {
 char *tmp=g_dir_make_tmp("xpwm-night-XXXXXX",NULL);
 g_setenv("XDG_CONFIG_HOME",tmp,TRUE);
 char *mock=g_build_filename(tmp,"wlsunset",NULL);
 g_file_set_contents(mock,"#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$XDG_CONFIG_HOME/args\"\n",-1,NULL);
 g_chmod(mock,0700); g_setenv("PATH",tmp,TRUE);
 GKeyFile *key=night_load(); g_assert_cmpint(night_number(key,"Sunrise",7,0,23),==,7);
 g_key_file_set_integer(key,"Night Light","Sunrise",999); g_assert_cmpint(night_number(key,"Sunrise",7,0,23),==,23);
 g_key_file_set_integer(key,"Night Light","Sunrise",7);
 g_key_file_set_boolean(key,"Night Light","Enabled",TRUE);
 char *path=night_path(), *dir=g_path_get_dirname(path), *data=g_key_file_to_data(key,NULL,NULL);
 g_mkdir_with_parents(dir,0700); g_file_set_contents(path,data,-1,NULL);
 night_start(); g_assert_nonnull(night_process); g_subprocess_wait(night_process,NULL,NULL);
 char *args=g_build_filename(tmp,"args",NULL), *actual=NULL; g_file_get_contents(args,&actual,NULL,NULL);
 g_assert_cmpstr(actual,==,"-S\n07:00\n-s\n19:00\n-t\n4000\n-T\n6500\n-d\n1800\n");
 night_stop(); g_assert_null(night_process);
 g_key_file_set_boolean(key,"Night Light","Enabled",FALSE); g_free(data); data=g_key_file_to_data(key,NULL,NULL); g_file_set_contents(path,data,-1,NULL);
 night_start(); g_assert_null(night_process); g_print("PASS: defaults, bounds, saved config, exact wlsunset arguments, disabled startup\n");
 return 0;
}
