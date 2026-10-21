#include "../shell/start-menu.c"
#include <glib/gstdio.h>
int main(int argc,char **argv)
{
 char *root=g_dir_make_tmp("xpwm-start-actions.XXXXXX",NULL),*config=g_build_filename(root,"config",NULL),*data=g_build_filename(root,"data",NULL);
 g_setenv("XDG_CONFIG_HOME",config,TRUE);g_setenv("XDG_DATA_HOME",data,TRUE);
 char *desktop=g_build_filename(root,"Desktop",NULL);g_mkdir_with_parents(config,0700);
 char *dirs=g_build_filename(config,"user-dirs.dirs",NULL),*settings=g_strdup_printf("XDG_DESKTOP_DIR=\"%s\"\n",desktop);g_file_set_contents(dirs,settings,-1,NULL);
 char *apps=g_build_filename(data,"applications",NULL);g_mkdir_with_parents(apps,0700);
 char *source=g_build_filename(apps,"xpwm-pin-test.desktop",NULL);
 const char *contents="[Desktop Entry]\nType=Application\nName=Pin Test\nExec=/bin/true %f\nIcon=utilities-terminal\nCategories=Utility;\n";
 g_assert_true(g_file_set_contents(source,contents,-1,NULL));
 GDesktopAppInfo *app=g_desktop_app_info_new_from_filename(source);g_assert_nonnull(app);
 const char *id=g_app_info_get_id(G_APP_INFO(app));GError *error=NULL;
 g_assert_true(set_pin(id,TRUE,&error));g_assert_no_error(error);g_assert_true(has_pin(id));
 g_assert_true(set_pin(id,TRUE,&error));char **ids=load_pins();g_assert_cmpuint(g_strv_length(ids),==,1);g_strfreev(ids);
 /* Resolve the persisted ID using the same path the next login uses. */
 GDesktopAppInfo *restored=g_desktop_app_info_new(id);g_assert_nonnull(restored);g_object_unref(restored);
 g_assert_true(desktop_shortcut(G_APP_INFO(app),&error));g_assert_no_error(error);
 char *target=g_build_filename(desktop,"xpwm-pin-test.desktop",NULL),*copy=NULL;g_file_get_contents(target,&copy,NULL,NULL);g_assert_cmpstr(copy,==,contents);g_free(copy);
 g_assert_false(desktop_shortcut(G_APP_INFO(app),&error));g_assert_error(error,G_IO_ERROR,G_IO_ERROR_EXISTS);g_clear_error(&error);
 g_assert_true(set_pin(id,FALSE,&error));g_assert_false(has_pin(id));
 g_assert_true(set_pin(id,TRUE,&error));g_object_unref(app);
 gtk_init(&argc,&argv);luna_init();GtkWidget *menu=start_menu_new();
 GList *children=gtk_container_get_children(GTK_CONTAINER(menu));int found=0;
 for(GList *l=children;l;l=l->next) {
  GAppInfo *entry=g_object_get_data(G_OBJECT(l->data),"application");
  if(entry && !g_strcmp0(g_app_info_get_id(entry),"xpwm-pin-test.desktop"))found++;
 }
 g_assert_cmpint(found,==,1);g_list_free(children);gtk_widget_destroy(menu);luna_finish();
 char *path=pins_path(),*parent=g_path_get_dirname(path);g_remove(path);g_rmdir(parent);g_free(parent);g_free(path);
 g_remove(target);g_rmdir(desktop);g_remove(source);g_rmdir(apps);g_rmdir(data);g_remove(dirs);g_rmdir(config);g_rmdir(root);
 g_free(root);g_free(config);g_free(data);g_free(desktop);g_free(dirs);g_free(settings);g_free(apps);g_free(source);g_free(target);
 g_print("PASS: persistent Start pins, deduplication, unpin, rendered custom pin, Desktop entry copy and existing-file protection\n");return 0;
}
