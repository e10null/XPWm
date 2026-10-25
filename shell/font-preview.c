/* Read-only GTK/Pango font comparison; never writes appearance preferences. */
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include "../shared/font-settings.h"
#include "../shared/luna.h"
#include "xp-style.h"
static GtkWidget *font, *dpi, *sample, *choice, *window;
static const char *names[] = {"Full hinting (smoothed)", "Slight hinting (lighter)", "No hinting", "No smoothing (pixelated)"};
static const cairo_hint_style_t hints[] = {CAIRO_HINT_STYLE_FULL, CAIRO_HINT_STYLE_SLIGHT, CAIRO_HINT_STYLE_NONE, CAIRO_HINT_STYLE_FULL};
static void text(cairo_t *cr, PangoLayout *layout, const char *value, double x, double y)
{
 pango_layout_set_text(layout,value,-1);cairo_move_to(cr,x,y);pango_cairo_show_layout(cr,layout);
}
static gboolean draw(GtkWidget *widget,cairo_t *cr,gpointer data)
{
 int style=GPOINTER_TO_INT(data),width=gtk_widget_get_allocated_width(widget);
 char *selected=gtk_font_chooser_get_font(GTK_FONT_CHOOSER(font));
 PangoFontDescription *description=pango_font_description_from_string(selected);g_free(selected);
 cairo_font_options_t *options=cairo_font_options_create();
 cairo_font_options_set_hint_style(options,hints[style]);
 cairo_font_options_set_antialias(options,style==3?CAIRO_ANTIALIAS_NONE:CAIRO_ANTIALIAS_GRAY);
 cairo_font_options_set_hint_metrics(options,CAIRO_HINT_METRICS_ON);
 PangoFontMap *map=pango_cairo_font_map_new();
 PangoContext *context=pango_font_map_create_context(map);
 pango_cairo_context_set_resolution(context,gtk_spin_button_get_value(GTK_SPIN_BUTTON(dpi)));
 pango_cairo_context_set_font_options(context,options);
 PangoLayout *layout=pango_layout_new(context);pango_layout_set_font_description(layout,description);
 pango_layout_set_width(layout,(width-32)*PANGO_SCALE);pango_layout_set_ellipsize(layout,PANGO_ELLIPSIZE_END);
 cairo_set_source_rgb(cr,.925,.914,.847);cairo_paint(cr);
 cairo_set_source_rgb(cr,0,0,0);
 text(cr,layout,gtk_entry_get_text(GTK_ENTRY(sample)),12,12);
 text(cr,layout,"File   Edit   View   Tools   Help",12,44);
 text(cr,layout,"The quick brown fox jumps over the lazy dog.",12,74);
 text(cr,layout,"0123456789  Aa Bb Gg Mm Ww  !?",12,100);
 cairo_set_source_rgb(cr,1,1,1);cairo_rectangle(cr,12,136,width-24,30);cairo_fill_preserve(cr);
 cairo_set_source_rgb(cr,.5,.6,.7);cairo_set_line_width(cr,1);cairo_stroke(cr);
 cairo_set_source_rgb(cr,0,0,0);text(cr,layout,"Open: C:\\Documents and Settings",18,141);
 cairo_rectangle(cr,12,184,13,13);cairo_set_source_rgb(cr,1,1,1);cairo_fill_preserve(cr);
 cairo_set_source_rgb(cr,.4,.4,.4);cairo_stroke(cr);
 cairo_set_source_rgb(cr,0,0,0);text(cr,layout,"Remember my settings",33,181);
 cairo_set_source_rgb(cr,.98,.98,.97);cairo_rectangle(cr,12,219,110,28);cairo_fill_preserve(cr);
 cairo_set_source_rgb(cr,.2,.35,.5);cairo_stroke(cr);cairo_set_source_rgb(cr,0,0,0);text(cr,layout,"OK / Apply",26,224);
 g_object_unref(layout);g_object_unref(context);g_object_unref(map);
 pango_font_description_free(description);cairo_font_options_destroy(options);return FALSE;
}
static void redraw(GtkWidget *widget,gpointer data){gtk_widget_queue_draw(window);}
static void copy(GtkWidget *widget,gpointer data)
{
 char *selected=gtk_font_chooser_get_font(GTK_FONT_CHOOSER(font));
 int index=gtk_combo_box_get_active(GTK_COMBO_BOX(choice));
 char *value=g_strdup_printf("Font: %s; DPI: %.0f; rendering: %s",selected,gtk_spin_button_get_value(GTK_SPIN_BUTTON(dpi)),names[index]);
 gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),value,-1);
 g_print("%s\n",value);g_free(value);g_free(selected);
 gtk_button_set_label(GTK_BUTTON(widget),"Choice copied");
}
int main(int argc,char **argv)
{
 gtk_init(&argc,&argv);luna_init();
 window=gtk_window_new(GTK_WINDOW_TOPLEVEL);gtk_window_set_title(GTK_WINDOW(window),"Font comparison — XPWm");
 gtk_window_set_default_size(GTK_WINDOW(window),840,750);gtk_container_set_border_width(GTK_CONTAINER(window),12);
 GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);gtk_container_add(GTK_CONTAINER(window),box);
 GtkWidget *note=gtk_label_new("Compare the same font and size with four rendering styles. Desktop settings are unchanged.");
 gtk_label_set_line_wrap(GTK_LABEL(note),TRUE);gtk_box_pack_start(GTK_BOX(box),note,FALSE,FALSE,0);
 GtkWidget *row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10);gtk_box_pack_start(GTK_BOX(box),row,FALSE,FALSE,0);
 font=gtk_font_button_new_with_font("Tahoma 8");gtk_font_button_set_show_style(GTK_FONT_BUTTON(font),TRUE);
 gtk_box_pack_start(GTK_BOX(row),gtk_label_new("Font:"),FALSE,FALSE,0);gtk_box_pack_start(GTK_BOX(row),font,TRUE,TRUE,0);
 dpi=gtk_spin_button_new_with_range(48,192,1);gtk_spin_button_set_value(GTK_SPIN_BUTTON(dpi),xp_font_dpi_read());
 gtk_box_pack_start(GTK_BOX(row),gtk_label_new("DPI:"),FALSE,FALSE,0);gtk_box_pack_start(GTK_BOX(row),dpi,FALSE,FALSE,0);
 sample=gtk_entry_new();gtk_entry_set_text(GTK_ENTRY(sample),"Windows XP — Display Properties");gtk_box_pack_start(GTK_BOX(box),sample,FALSE,FALSE,0);
 GtkWidget *grid=gtk_grid_new();gtk_grid_set_row_spacing(GTK_GRID(grid),10);gtk_grid_set_column_spacing(GTK_GRID(grid),10);
 gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);gtk_box_pack_start(GTK_BOX(box),grid,TRUE,TRUE,0);
 for(int i=0;i<4;i++) {
  GtkWidget *frame=gtk_frame_new(names[i]),*area=gtk_drawing_area_new();gtk_widget_set_size_request(area,360,260);
  gtk_container_add(GTK_CONTAINER(frame),area);gtk_widget_set_hexpand(frame,TRUE);gtk_widget_set_vexpand(frame,TRUE);
  gtk_grid_attach(GTK_GRID(grid),frame,i%2,i/2,1,1);g_signal_connect(area,"draw",G_CALLBACK(draw),GINT_TO_POINTER(i));
 }
 row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10);gtk_box_pack_start(GTK_BOX(box),row,FALSE,FALSE,0);
 choice=gtk_combo_box_text_new();for(int i=0;i<4;i++)gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(choice),names[i]);gtk_combo_box_set_active(GTK_COMBO_BOX(choice),3);
 gtk_box_pack_start(GTK_BOX(row),choice,TRUE,TRUE,0);GtkWidget *button=gtk_button_new_with_label("Copy my choice");gtk_box_pack_start(GTK_BOX(row),button,FALSE,FALSE,0);
 g_signal_connect(button,"clicked",G_CALLBACK(copy),NULL);g_signal_connect(font,"font-set",G_CALLBACK(redraw),NULL);
 g_signal_connect(dpi,"value-changed",G_CALLBACK(redraw),NULL);g_signal_connect(sample,"changed",G_CALLBACK(redraw),NULL);
 g_signal_connect(window,"destroy",G_CALLBACK(gtk_main_quit),NULL);xp_style_apply(window);gtk_widget_show_all(window);gtk_main();luna_finish();return 0;
}
