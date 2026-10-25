#include "run-suggestions.h"
#include <string.h>
#include <gio/gdesktopappinfo.h>

static void suggestion_free(gpointer data)
{
 RunSuggestion *s=data;g_free(s->command);g_free(s->folded);g_free(s->label);g_free(s->search);g_free(s);
}
static void add(GPtrArray *items,GHashTable *seen,const char *command,int recent)
{
 if(!*command || !g_utf8_validate(command,-1,NULL) || g_hash_table_contains(seen,command))return;
 RunSuggestion *s=g_new0(RunSuggestion,1);s->command=g_strdup(command);
 s->label=g_strdup(command);s->folded=g_utf8_casefold(command,-1);s->recent=recent;
 g_ptr_array_add(items,s);g_hash_table_add(seen,s->command);
}
GPtrArray *run_suggestions_scan(const char *path,char **history,GCancellable *cancel)
{
 GPtrArray *items=g_ptr_array_new_with_free_func(suggestion_free);
 GHashTable *seen=g_hash_table_new(g_str_hash,g_str_equal);
 for(guint i=0;history && history[i];i++)add(items,seen,history[i],i);
 char **dirs=g_strsplit(path ? path : "",G_SEARCHPATH_SEPARATOR_S,-1);
 for(guint i=0;dirs[i] && !g_cancellable_is_cancelled(cancel);i++) {
  /* Run executes in HOME, so empty/relative PATH components use that directory. */
  char *directory=g_canonicalize_filename(*dirs[i] ? dirs[i] : ".",g_get_home_dir());
  GDir *dir=g_dir_open(directory,0,NULL);const char *name;
  while(dir && !g_cancellable_is_cancelled(cancel) && (name=g_dir_read_name(dir))) {
   if(!g_utf8_validate(name,-1,NULL))continue;
   char *file=g_build_filename(directory,name,NULL);
   if(g_file_test(file,G_FILE_TEST_IS_REGULAR) && g_file_test(file,G_FILE_TEST_IS_EXECUTABLE)) {
    char *command=strspn(name,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.+")==strlen(name)
     ? g_strdup(name) : g_shell_quote(name);
    add(items,seen,command,-1);g_free(command);
   }
   g_free(file);
  }
  if(dir)g_dir_close(dir);
  g_free(directory);
 }
 g_strfreev(dirs);g_hash_table_unref(seen);return items;
}
void run_suggestions_add_apps(GPtrArray *items,GCancellable *cancel)
{
 GHashTable *by_command=g_hash_table_new(g_str_hash,g_str_equal);
 for(guint i=0;i<items->len;i++){RunSuggestion *s=g_ptr_array_index(items,i);g_hash_table_insert(by_command,s->command,s);}
 GList *apps=g_app_info_get_all();
 for(GList *l=apps;l && !g_cancellable_is_cancelled(cancel);l=l->next) {
  GAppInfo *app=l->data;
  if(!G_IS_DESKTOP_APP_INFO(app) || !g_app_info_should_show(app))continue;
  const char *file=g_desktop_app_info_get_filename(G_DESKTOP_APP_INFO(app));
  const char *name=g_app_info_get_display_name(app),*executable=g_app_info_get_executable(app);
  if(!file || !name)continue;
  char *command=g_shell_quote(file);
  RunSuggestion *s=g_hash_table_lookup(by_command,command);
  if(!s){s=g_new0(RunSuggestion,1);s->command=g_strdup(command);s->recent=-1;g_ptr_array_add(items,s);g_hash_table_insert(by_command,s->command,s);}
  g_free(command);g_free(s->label);g_free(s->folded);g_free(s->search);
  s->label=g_strdup(name);s->folded=g_utf8_casefold(name,-1);
  const char * const *keys=g_desktop_app_info_get_keywords(G_DESKTOP_APP_INFO(app));
  char *keywords=keys?g_strjoinv(" ",(char **)keys):g_strdup("");
  const char *generic=g_desktop_app_info_get_generic_name(G_DESKTOP_APP_INFO(app));
  char *search=g_strjoin(" ",name,executable?executable:"",generic?generic:"",keywords,NULL);
  s->search=g_utf8_casefold(search,-1);g_free(search);g_free(keywords);
 }
 g_list_free_full(apps,g_object_unref);g_hash_table_unref(by_command);
}
typedef struct {RunSuggestion *item;gint64 score;} Match;
static gint compare(gconstpointer a,gconstpointer b)
{
 const Match *x=a,*y=b;
 if(x->score!=y->score)return x->score<y->score ? -1 : 1;
 return g_strcmp0(x->item->command,y->item->command);
}
static gint64 score(const RunSuggestion *s,const char *query)
{
 const char *value=s->folded;
 gint64 rank;
 if(!*query)return s->recent>=0 ? s->recent : 100000;
 if(!strcmp(value,query))return 0;
 if(g_str_has_prefix(value,query))rank=1000+g_utf8_strlen(value,-1);
 else if(strstr(value,query))rank=10000+g_utf8_strlen(value,-1);
 else {
  /* Arguments and paths complete from history; executable names use subsequences. */
  if(strpbrk(query," \t/"))return -1;
  const char *p=value;int gaps=0;
  for(const char *q=query;*q;q=g_utf8_next_char(q)) {
   gunichar ch=g_utf8_get_char(q);
   while(*p && g_utf8_get_char(p)!=ch){p=g_utf8_next_char(p);gaps++;}
   if(!*p)return -1;
   p=g_utf8_next_char(p);
  }
  rank=20000+gaps*10+g_utf8_strlen(value,-1);
 }
 return rank+(s->recent>=0 ? MIN(s->recent,99) : 100);
}
static gint64 app_score(const RunSuggestion *item,const char *query)
{
 gint64 best=score(item,query);
 if(!item->search || !*query)return best;
 RunSuggestion alias={.folded=item->search,.recent=item->recent};
 gint64 rank=score(&alias,query);
 if(rank>=0 && (best<0 || rank<best))best=rank;
 if(best<0 && strpbrk(query," \t")) {
  char **words=g_strsplit_set(query," \t",-1);gint64 total=30000;
  for(guint i=0;words[i];i++)if(*words[i]) {
   rank=score(&alias,words[i]);
   if(rank<0){total=-1;break;}
   total+=rank;
  }
  g_strfreev(words);best=total;
 }
 return best;
}
GPtrArray *run_suggestions_match(GPtrArray *items,const char *query,guint limit)
{
 char *folded=g_utf8_casefold(query,-1);g_strstrip(folded);
 GArray *matches=g_array_new(FALSE,FALSE,sizeof(Match));
 for(guint i=0;i<items->len;i++) {
  RunSuggestion *item=g_ptr_array_index(items,i);gint64 rank=app_score(item,folded);
  if(rank>=0){Match match={item,rank};g_array_append_val(matches,match);}
 }
 g_array_sort(matches,compare);GPtrArray *result=g_ptr_array_new();
 for(guint i=0;i<MIN(limit,matches->len);i++)g_ptr_array_add(result,g_array_index(matches,Match,i).item);
 g_array_unref(matches);g_free(folded);return result;
}
