#include "screensaver-settings.h"
#include "screensaver-view.h"
#include "xp-style.h"
typedef struct {XpSaverConfig saved;GtkWidget *combo,*lock,*lock_minutes,*minutes,*text,*pictures,*view,*preview;void (*changed)(gpointer);gpointer data;} SaverSettings;
static XpSaverConfig draft(SaverSettings *s)
{return (XpSaverConfig){gtk_combo_box_get_active(GTK_COMBO_BOX(s->combo)),gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(s->minutes)),
 g_strdup(gtk_entry_get_text(GTK_ENTRY(s->text))),g_strdup(gtk_entry_get_text(GTK_ENTRY(s->pictures))),gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s->lock)),gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(s->lock_minutes))};}
static void settings_free(gpointer data) {SaverSettings *s=data;xp_saver_config_clear(&s->saved);g_free(s);}
static void changed(GtkWidget *widget,gpointer data)
{
 SaverSettings *s=data;XpSaverConfig c=draft(s);(void)widget;xp_saver_view_configure(s->view,&c);
 gboolean text=c.mode==XP_SAVER_TEXT || c.mode==XP_SAVER_MARQUEE;
 gtk_widget_set_sensitive(s->text,text);gtk_widget_set_sensitive(s->pictures,c.mode==XP_SAVER_PICTURES);
 gtk_widget_set_sensitive(s->preview,c.mode!=XP_SAVER_NONE);gtk_widget_set_sensitive(s->lock_minutes,c.lock);xp_saver_config_clear(&c);
 if(s->changed)s->changed(s->data);
}
gboolean xp_saver_settings_dirty(GtkWidget *page)
{
 SaverSettings *s=g_object_get_data(G_OBJECT(page),"saver-settings");XpSaverConfig c=draft(s);
 gboolean result=c.lock_minutes!=s->saved.lock_minutes || c.lock!=s->saved.lock || c.mode!=s->saved.mode || c.minutes!=s->saved.minutes || g_strcmp0(c.text,s->saved.text) || g_strcmp0(c.pictures,s->saved.pictures);
 xp_saver_config_clear(&c);return result;
}
gboolean xp_saver_settings_apply(GtkWidget *page,GError **error)
{
 SaverSettings *s=g_object_get_data(G_OBJECT(page),"saver-settings");XpSaverConfig c=draft(s);
 char *locker=g_find_program_in_path("swaylock");
 if(c.lock && !locker) {
  g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_NOT_FOUND,"Secure locking requires swaylock. Install it first: doas emerge --ask gui-apps/swaylock");
  xp_saver_config_clear(&c);return FALSE;
 }
 g_free(locker);
 gboolean ok=xp_saver_config_save(&c,error);
 if(ok){xp_saver_config_clear(&s->saved);xp_saver_config_copy(&s->saved,&c);}
 xp_saver_config_clear(&c);return ok;
}
static void preview(GtkButton *button,gpointer data)
{
 SaverSettings *s=data;XpSaverConfig c=draft(s);GError *error=NULL;(void)button;
 const char *root=g_getenv("XPWM_PROJECT_ROOT");char *program=g_build_filename(root?root:".","shell","xpwm-screensaver",NULL);
 char *mode=g_strdup_printf("--mode=%d",c.mode);
 GSubprocess *process=g_subprocess_new(G_SUBPROCESS_FLAGS_NONE,&error,program,mode,"--text",c.text,"--pictures",c.pictures,NULL);
 if(process)g_object_unref(process);
 else {
  GtkWidget *message=gtk_message_dialog_new(GTK_WINDOW(gtk_widget_get_toplevel(s->combo)),GTK_DIALOG_DESTROY_WITH_PARENT,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,"Unable to preview screen saver");
  gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(message),"%s",error->message);
  g_signal_connect_swapped(message,"response",G_CALLBACK(gtk_widget_destroy),message);xp_style_apply(message);gtk_widget_show(message);g_clear_error(&error);
 }
 g_free(program);g_free(mode);xp_saver_config_clear(&c);
}
static void labelled(GtkWidget *box,const char *title,GtkWidget *widget)
{
 GtkWidget *row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8),*label=gtk_label_new_with_mnemonic(title);
 gtk_label_set_mnemonic_widget(GTK_LABEL(label),widget);gtk_box_pack_start(GTK_BOX(row),label,FALSE,FALSE,0);
 gtk_box_pack_start(GTK_BOX(row),widget,TRUE,TRUE,0);gtk_box_pack_start(GTK_BOX(box),row,FALSE,FALSE,0);
}
GtkWidget *xp_saver_settings_new(void (*notify)(gpointer),gpointer data)
{
 SaverSettings *s=g_new0(SaverSettings,1);xp_saver_config_load(&s->saved);s->changed=notify;s->data=data;
 GtkWidget *page=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);gtk_container_set_border_width(GTK_CONTAINER(page),12);
 g_object_set_data_full(G_OBJECT(page),"saver-settings",s,settings_free);
 s->view=xp_saver_view_new(&s->saved);gtk_widget_set_size_request(s->view,280,170);
 GtkWidget *frame=gtk_frame_new(NULL);gtk_container_add(GTK_CONTAINER(frame),s->view);gtk_box_pack_start(GTK_BOX(page),frame,FALSE,FALSE,0);
 s->combo=gtk_combo_box_text_new();for(int i=0;i<XP_SAVER_COUNT;i++)gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(s->combo),xp_saver_names[i]);
 gtk_combo_box_set_active(GTK_COMBO_BOX(s->combo),s->saved.mode);labelled(page,"_Screen saver:",s->combo);
 s->preview=gtk_button_new_with_mnemonic("_Preview");gtk_box_pack_start(GTK_BOX(page),s->preview,FALSE,FALSE,0);
 s->minutes=gtk_spin_button_new_with_range(1,120,1);gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->minutes),s->saved.minutes);labelled(page,"Saver _wait (minutes):",s->minutes);
 s->lock=gtk_check_button_new_with_label("Lock automatically");
 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->lock),s->saved.lock);
 gtk_box_pack_start(GTK_BOX(page),s->lock,FALSE,FALSE,0);
 s->lock_minutes=gtk_spin_button_new_with_range(1,120,1);
 gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->lock_minutes),s->saved.lock_minutes);
 labelled(page,"Lock after (minutes):",s->lock_minutes);
 g_signal_connect(s->lock_minutes,"value-changed",G_CALLBACK(changed),s);
 g_signal_connect(s->lock,"toggled",G_CALLBACK(changed),s);
 s->text=gtk_entry_new();gtk_entry_set_max_length(GTK_ENTRY(s->text),64);gtk_entry_set_text(GTK_ENTRY(s->text),s->saved.text);labelled(page,"_Text:",s->text);
 s->pictures=gtk_entry_new();gtk_entry_set_text(GTK_ENTRY(s->pictures),s->saved.pictures);labelled(page,"Pictures _folder:",s->pictures);
 g_signal_connect(s->combo,"changed",G_CALLBACK(changed),s);g_signal_connect(s->minutes,"value-changed",G_CALLBACK(changed),s);
 g_signal_connect(s->text,"changed",G_CALLBACK(changed),s);g_signal_connect(s->pictures,"changed",G_CALLBACK(changed),s);
 g_signal_connect(s->preview,"clicked",G_CALLBACK(preview),s);changed(NULL,s);return page;
}
