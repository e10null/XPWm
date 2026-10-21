#include "../shell/run-suggestions.h"
#include <glib/gstdio.h>
static void file(const char *dir,const char *name,int mode)
{char *path=g_build_filename(dir,name,NULL);g_assert_true(g_file_set_contents(path,"#!/bin/sh\n",-1,NULL));g_assert_cmpint(g_chmod(path,mode),==,0);g_free(path);}
int main(void)
{
 char *dir=g_dir_make_tmp("xpwm-suggestions.XXXXXX",NULL);
 file(dir,"firefox",0700);file(dir,"filer",0700);file(dir,"firefox-beta",0700);
 file(dir,"firefox-no-exec",0600);file(dir,"space command",0700);
 char *subdir=g_build_filename(dir,"firefox-directory",NULL);g_mkdir(subdir,0700);
 char *history[]={"firefox --private-window","firefox",NULL};
 char *path=g_strjoin(G_SEARCHPATH_SEPARATOR_S,dir,dir,"/missing/xpwm",NULL);
 GPtrArray *items=run_suggestions_scan(path,history,NULL);
 g_assert_cmpuint(items->len,==,5);
 GPtrArray *matches=run_suggestions_match(items,"FIREFOX",30);
 g_assert_cmpstr(((RunSuggestion *)g_ptr_array_index(matches,0))->command,==,"firefox");g_ptr_array_unref(matches);
 matches=run_suggestions_match(items,"ffx",30);g_assert_cmpuint(matches->len,>=,1);g_ptr_array_unref(matches);
 matches=run_suggestions_match(items,"firefox --pr",30);
 g_assert_cmpuint(matches->len,==,1);g_assert_cmpstr(((RunSuggestion *)g_ptr_array_index(matches,0))->command,==,history[0]);g_ptr_array_unref(matches);
 matches=run_suggestions_match(items,"no-match",30);g_assert_cmpuint(matches->len,==,0);g_ptr_array_unref(matches);
 matches=run_suggestions_match(items,"space",1);
 g_assert_cmpstr(((RunSuggestion *)g_ptr_array_index(matches,0))->command,==,"'space command'");g_ptr_array_unref(matches);
 g_ptr_array_unref(items);g_free(path);g_rmdir(subdir);g_free(subdir);
 GDir *listing=g_dir_open(dir,0,NULL);const char *name;
 while((name=g_dir_read_name(listing))){path=g_build_filename(dir,name,NULL);g_remove(path);g_free(path);}
 g_dir_close(listing);g_rmdir(dir);g_free(dir);
 g_print("PASS: executable discovery, duplicate PATH entries, history, exact/prefix/fuzzy matching, case folding and quoted executable names\n");return 0;
}
