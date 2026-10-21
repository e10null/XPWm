#define main desktop_main
#include "../shell/desktop.c"
#undef main
static gboolean popup_fixture(gpointer data)
{
 static int phase;
 if(!window || loading)return G_SOURCE_CONTINUE;
 if(!phase){properties(NULL,NULL);phase++;return G_SOURCE_CONTINUE;}
 GtkWidget *combo=g_object_get_data(G_OBJECT(properties_window),"wallpaper-mode");
 int x,y;gtk_widget_translate_coordinates(combo,properties_window,0,0,&x,&y);
 char *text=g_strdup_printf("%d %d %d %d",x,y,gtk_widget_get_allocated_width(combo),gtk_widget_get_allocated_height(combo));
 char *path=g_build_filename(g_getenv("XDG_RUNTIME_DIR"),"popup-anchor",NULL);g_file_set_contents(path,text,-1,NULL);g_free(path);g_free(text);
 return G_SOURCE_REMOVE;
}
int main(int argc,char **argv)
{g_timeout_add(150,popup_fixture,NULL);return desktop_main(argc,argv);}
