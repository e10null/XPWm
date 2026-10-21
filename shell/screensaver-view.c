/* Native Cairo recreations. No Windows code, external renderers or idle daemon. */
#include "screensaver-view.h"
#include "../shared/luna.h"
#include <math.h>
#include <pango/pangocairo.h>
#include "gentoo-ascii.h"

typedef struct {
 XpSaverConfig config;
 gint64 started, next_photo;
 guint timer, generation, photo_index;
 gboolean dead, loading;
 gint64 last_frame;
 GCancellable *cancel;
 GdkPixbuf *photo;
 PangoLayout *ascii, *ascii_light;
 cairo_surface_t *pipes_cache, *frame;
 int pipes_cycle, pipes_steps, pipes_width, pipes_height;
} SaverView;
typedef struct { double x, y, z; } Point3;
typedef struct { double x[4], y[4], depth, shade, hue; } Face;
static double fract(double x) { return x-floor(x); }
static double random_at(int n) { return fract(sin(n*127.1+311.7)*43758.5453); }
static double bounce(double x) { return 1-fabs(fmod(fabs(x), 2)-1); }
static void color(cairo_t *cr, double hue, double brightness, double alpha)
{
 double h = fract(hue)*6, x = brightness*(1-fabs(fmod(h, 2)-1));
 double r=0, g=0, b=0;
 switch ((int)h) { case 0:r=brightness;g=x;break;case 1:r=x;g=brightness;break;
 case 2:g=brightness;b=x;break;case 3:g=x;b=brightness;break;case 4:r=x;b=brightness;break;default:r=brightness;b=x; }
 cairo_set_source_rgba(cr, r, g, b, alpha);
}
static void text(cairo_t *cr, const char *value, double size, double x, double y)
{
 PangoLayout *layout = pango_cairo_create_layout(cr);
 PangoFontDescription *font = pango_font_description_new();
 pango_font_description_set_family(font, "Tahoma");
 pango_font_description_set_absolute_size(font, size*PANGO_SCALE);
 pango_font_description_set_weight(font, PANGO_WEIGHT_BOLD);
 pango_layout_set_font_description(layout, font); pango_layout_set_text(layout, value, -1);
 cairo_move_to(cr, x, y); pango_cairo_show_layout(cr, layout);
 g_object_unref(layout); pango_font_description_free(font);
}
static Point3 rotate(Point3 p, double t)
{
 double x = p.x*cos(t)-p.z*sin(t), z = p.x*sin(t)+p.z*cos(t);
 return (Point3){x, p.y*cos(t*.63)-z*sin(t*.63), p.y*sin(t*.63)+z*cos(t*.63)};
}
static void project(Point3 p, double w, double h, double *x, double *y)
{ double scale = MIN(w,h)*.36/(3.8+p.z); *x=w/2+p.x*scale; *y=h/2+p.y*scale; }
static int face_depth(gconstpointer a, gconstpointer b)
{ double d=((const Face *)b)->depth-((const Face *)a)->depth; return d<0 ? -1 : d>0; }
static void flower_or_flag(cairo_t *cr, int mode, double w, double h, double t)
{
 Face faces[192]; int count=0;
 for (int row=0; row<12; row++) for (int col=0; col<16; col++) {
  Face *f=&faces[count++]; f->depth=0; f->hue=fract(col/16.+t*.025); f->shade=.55+.45*row/12.;
  for (int corner=0; corner<4; corner++) {
   double u=(col+(corner==1 || corner==2))/16., v=(row+(corner>=2))/12.; Point3 p;
   if (mode==XP_SAVER_FLYING) {
    p=(Point3){(u-.5)*4, (v-.5)*2.8, .23*sin(u*8+t*2)+.12*cos(v*7+t)};
    f->hue = u<.5 ? (v<.5 ? .025 : .60) : (v<.5 ? .27 : .13);
   } else {
    double a=u*2*G_PI, b=v*G_PI;
    double r=1.5+.4*sin(t*.7)*cos(a*4)*sin(b*3);
    p=(Point3){r*sin(b)*cos(a), r*cos(b), r*sin(b)*sin(a)};
   }
   p=rotate(p, t*.3); f->depth+=p.z/4; project(p,w,h,&f->x[corner],&f->y[corner]);
  }
 }
 qsort(faces,count,sizeof(Face),face_depth);
 for (int i=0;i<count;i++) {
  Face *f=&faces[i]; cairo_move_to(cr,f->x[0],f->y[0]);
  for (int j=1;j<4;j++) cairo_line_to(cr,f->x[j],f->y[j]);
  cairo_close_path(cr); color(cr,f->hue,f->shade,1); cairo_fill_preserve(cr);
  cairo_set_source_rgba(cr,0,0,0,.12); cairo_set_line_width(cr,.5); cairo_stroke(cr);
 }
}
static void pipe_segment(cairo_t *cr, Point3 p, Point3 next, double w, double h, double hue)
{
 Point3 a=rotate(p,.6), b=rotate(next,.6); double x,y,x2,y2;
 project(a,w,h,&x,&y);project(b,w,h,&x2,&y2);
 double width=MIN(w,h)*.026/(3.8+b.z);
 cairo_set_line_cap(cr,CAIRO_LINE_CAP_ROUND);
 cairo_set_line_width(cr,width+2);color(cr,hue,.3,1);
 cairo_move_to(cr,x,y);cairo_line_to(cr,x2,y2);cairo_stroke(cr);
 cairo_set_line_width(cr,width*.6);color(cr,hue,.95,1);
 cairo_move_to(cr,x-width*.1,y-width*.1);cairo_line_to(cr,x2-width*.1,y2-width*.1);cairo_stroke(cr);
}
static Point3 pipe_next(Point3 p, int i, int pipe, int cycle)
{
 int axis=(int)(random_at(i+pipe*137+cycle*73)*3);
 double direction=random_at(i+pipe*71+cycle*191+900)>.5 ? .45 : -.45;
 double *coordinate=axis==0 ? &p.x : axis==1 ? &p.y : &p.z;
 if(*coordinate+direction < -2.2 || *coordinate+direction > 2.2)direction=-direction;
 *coordinate+=direction;return p;
}
static void pipes(SaverView *s, cairo_t *cr, double w, double h, double t)
{
 int cycle=(int)(t/18), steps=(int)(fmod(t,18)*7);
 double progress=MIN(1.,fmod(t,18)*7-steps);
 gboolean rebuild=!s->pipes_cache || s->pipes_width!=(int)w || s->pipes_height!=(int)h || s->pipes_cycle!=cycle || s->pipes_steps!=steps;
 if(rebuild) {
  if(s->pipes_cache)cairo_surface_destroy(s->pipes_cache);
  /* Bound software raster work on high resolution/HiDPI displays. */
  double scale=MIN(1.,960./MAX(w,h));
  s->pipes_cache=cairo_image_surface_create(CAIRO_FORMAT_RGB24,MAX(1,(int)ceil(w*scale)),MAX(1,(int)ceil(h*scale)));
  cairo_t *cache=cairo_create(s->pipes_cache);cairo_scale(cache,scale,scale);
  cairo_set_source_rgb(cache,0,0,0);cairo_paint(cache);
  for(int pipe=0;pipe<3;pipe++) {
   Point3 p={(pipe-1)*1.1,-1.3,0};
   for(int i=0;i<steps;i++) {Point3 next=pipe_next(p,i,pipe,cycle);pipe_segment(cache,p,next,w,h,pipe*.31+cycle*.13);p=next;}
  }
  cairo_destroy(cache);s->pipes_width=w;s->pipes_height=h;s->pipes_cycle=cycle;s->pipes_steps=steps;
 }
 cairo_save(cr);
 cairo_scale(cr,w/cairo_image_surface_get_width(s->pipes_cache),h/cairo_image_surface_get_height(s->pipes_cache));
 cairo_set_source_surface(cr,s->pipes_cache,0,0);cairo_pattern_set_filter(cairo_get_source(cr),CAIRO_FILTER_FAST);cairo_paint(cr);cairo_restore(cr);
 for(int pipe=0;pipe<3;pipe++) {
  Point3 p={(pipe-1)*1.1,-1.3,0};for(int i=0;i<steps;i++)p=pipe_next(p,i,pipe,cycle);
  Point3 next=pipe_next(p,steps,pipe,cycle);
  next=(Point3){p.x+(next.x-p.x)*progress,p.y+(next.y-p.y)*progress,p.z+(next.z-p.z)*progress};
  if(progress>0)pipe_segment(cr,p,next,w,h,pipe*.31+cycle*.13);
 }
}
/* Cache the monospace layout; each frame only translates and colors it. */
static void gentoo_dvd(SaverView *s,cairo_t *cr,double w,double h,double t)
{
 if(!s->ascii) {
  s->ascii=pango_cairo_create_layout(cr);
  PangoFontDescription *font=pango_font_description_from_string("Courier New Bold 18");
  pango_layout_set_font_description(s->ascii,font);
  pango_layout_set_text(s->ascii,gentoo_ascii,-1);
  s->ascii_light=pango_layout_copy(s->ascii);
  pango_layout_set_text(s->ascii_light,gentoo_ascii_light,-1);
  pango_font_description_free(font);
 }
 int aw,ah,lw,lh;pango_layout_get_pixel_size(s->ascii,&aw,&ah);
 pango_layout_get_pixel_size(s->ascii_light,&lw,&lh);aw=MAX(aw,lw);ah=MAX(ah,lh);
 double scale=MIN(1.,MIN(w*.48/aw,h*.55/ah));
 double travel_x=MAX(1.,w-aw*scale-8),travel_y=MAX(1.,h-ah*scale-8);
 double tx=t*85/travel_x,ty=t*61/travel_y;
 int hit=(int)floor(tx)+(int)floor(ty);
 double hue=fract(.73+hit*.173);
 cairo_save(cr);cairo_translate(cr,4+bounce(tx)*travel_x,4+bounce(ty)*travel_y);cairo_scale(cr,scale,scale);
 cairo_pattern_t *gradient=cairo_pattern_create_linear(0,0,aw,ah);
 color(cr,hue,.95,1);double r,g,b,a;cairo_pattern_get_rgba(cairo_get_source(cr),&r,&g,&b,&a);
 cairo_pattern_add_color_stop_rgb(gradient,0,r,g,b);
 color(cr,hue+.16,.85,1);cairo_pattern_get_rgba(cairo_get_source(cr),&r,&g,&b,&a);
 cairo_pattern_add_color_stop_rgb(gradient,1,r,g,b);
 cairo_set_source(cr,gradient);cairo_move_to(cr,0,0);pango_cairo_show_layout(cr,s->ascii);
 cairo_pattern_destroy(gradient);
 cairo_set_source_rgb(cr,.92,.91,.98);cairo_move_to(cr,0,0);pango_cairo_show_layout(cr,s->ascii_light);
 cairo_restore(cr);
}
static void draw_effect_raw(SaverView *s, cairo_t *cr, int width, int height, double t)
{
 int mode=s->config.mode; double w=width,h=height;
 cairo_set_source_rgb(cr,0,0,0);cairo_paint(cr);cairo_set_line_width(cr,1.2);
 if (mode==XP_SAVER_NONE || mode==XP_SAVER_BLANK) return;
 if (mode==XP_SAVER_FLOWER || mode==XP_SAVER_FLYING) { flower_or_flag(cr,mode,w,h,t); return; }
 if (mode==XP_SAVER_GENTOO) { gentoo_dvd(s,cr,w,h,t); return; }
 if (mode==XP_SAVER_PIPES) { pipes(s,cr,w,h,t); return; }
 if (mode==XP_SAVER_STARS) {
  for(int i=0;i<180;i++) {
   double z=.04+fract(random_at(i+900)+1-t*.16)*.96;
   double x=(random_at(i*3)-.5)*w/z+w/2, y=(random_at(i*3+1)-.5)*h/z+h/2;
   if(x<0 || x>w || y<0 || y>h) continue;
   cairo_set_source_rgb(cr,1-z*.6,1-z*.6,1-z*.5);
   cairo_arc(cr,x,y,MAX(.5,(1-z)*2.2),0,2*G_PI);cairo_fill(cr);
  }
 } else if(mode==XP_SAVER_MYSTIFY || mode==XP_SAVER_BEZIERS) {
  for(int trail=18;trail>=0;trail--) for(int figure=0;figure<2;figure++) {
   double x[4],y[4],time=t-trail*.055;
   for(int p=0;p<4;p++) {x[p]=(0.05+.9*bounce(time*(.08+random_at(p+figure*4)*.08)+random_at(p+33)))*w;
    y[p]=(0.05+.9*bounce(time*(.1+random_at(p+figure*7)*.08)+random_at(p+66)))*h;}
   color(cr,figure*.44+t*.015,(19-trail)/19.,1);cairo_move_to(cr,x[0],y[0]);
   if(mode==XP_SAVER_BEZIERS) cairo_curve_to(cr,x[1],y[1],x[2],y[2],x[3],y[3]);
   else {for(int p=1;p<4;p++)cairo_line_to(cr,x[p],y[p]);cairo_close_path(cr);}
   cairo_stroke(cr);
  }
 } else if(mode==XP_SAVER_TEXT || mode==XP_SAVER_MARQUEE) {
  const char *value=s->config.text && *s->config.text ? s->config.text : "Windows XP";
  double size=MIN(w/14,h/5), text_width=g_utf8_strlen(value,-1)*size*.65;
  if(mode==XP_SAVER_MARQUEE) {color(cr,.32,.9,1);text(cr,value,size,w-fmod(t*w*.1,w+text_width),h*.45);}
  else {
   cairo_save(cr);cairo_translate(cr,w/2,h/2);cairo_rotate(cr,.16*sin(t*.4));
   cairo_scale(cr,.3+.7*fabs(cos(t*.3)),1);
   for(int depth=18;depth>0;depth--) {color(cr,t*.025,.2+depth*.012,1);text(cr,value,size,-text_width/2+depth*.7,-size/2+depth*.6);}
   color(cr,t*.025,.95,1);text(cr,value,size,-text_width/2,-size/2);cairo_restore(cr);
  }
 } else if(mode==XP_SAVER_LOGO) {
  int cycle=(int)(t/8);double alpha=MIN(1.,MIN(fmod(t,8),8-fmod(t,8)));
  double size=MIN(w/260.,h/110.),x=random_at(cycle+24)*MAX(0,w-245*size),y=random_at(cycle+91)*MAX(0,h-90*size);
  cairo_save(cr);cairo_translate(cr,x,y);cairo_scale(cr,size,size);cairo_push_group(cr);
  cairo_save(cr);cairo_translate(cr,10,5);cairo_scale(cr,1.5,1.5);xp_start_flag(cr,0,0);cairo_restore(cr);
  cairo_set_source_rgb(cr,1,1,1);text(cr,"Microsoft",11,65,14);text(cr,"Windows",28,65,28);
  cairo_set_source_rgb(cr,1,.4,.1);text(cr,"xp",20,205,22);
  cairo_pop_group_to_source(cr);cairo_paint_with_alpha(cr,alpha);cairo_restore(cr);
 } else if(mode==XP_SAVER_PICTURES) {
  if(s->photo) {
   double pw=gdk_pixbuf_get_width(s->photo),ph=gdk_pixbuf_get_height(s->photo),scale=MIN(w/pw,h/ph);
   cairo_save(cr);cairo_translate(cr,(w-pw*scale)/2,(h-ph*scale)/2);cairo_scale(cr,scale,scale);
   gdk_cairo_set_source_pixbuf(cr,s->photo,0,0);cairo_paint(cr);cairo_restore(cr);
  } else {cairo_set_source_rgb(cr,.8,.8,.8);text(cr,"My Pictures",MIN(w/14,h/8),w*.2,h*.4);}
 }
}

/* Keep software effects within a fixed pixel budget, independent of output DPI. */
static void draw_effect(SaverView *s,cairo_t *cr,int width,int height,double t)
{
 int mode=s->config.mode;
 if(MAX(width,height)<=1280 || mode==XP_SAVER_PICTURES || mode==XP_SAVER_GENTOO || mode==XP_SAVER_BLANK || mode==XP_SAVER_NONE) {
  draw_effect_raw(s,cr,width,height,t);return;
 }
 double scale=1280./MAX(width,height);int w=MAX(1,(int)ceil(width*scale)),h=MAX(1,(int)ceil(height*scale));
 if(s->frame && (cairo_image_surface_get_width(s->frame)!=w || cairo_image_surface_get_height(s->frame)!=h)) {cairo_surface_destroy(s->frame);s->frame=NULL;}
 if(!s->frame)s->frame=cairo_image_surface_create(CAIRO_FORMAT_RGB24,w,h);
 cairo_t *frame=cairo_create(s->frame);draw_effect_raw(s,frame,w,h,t);cairo_destroy(frame);
 cairo_save(cr);cairo_scale(cr,(double)width/w,(double)height/h);cairo_set_source_surface(cr,s->frame,0,0);cairo_pattern_set_filter(cairo_get_source(cr),CAIRO_FILTER_FAST);cairo_paint(cr);cairo_restore(cr);
}

typedef struct {char *folder; guint index,generation; int width,height;} PhotoJob;
static void photo_job_free(gpointer data) {PhotoJob *j=data;g_free(j->folder);g_free(j);}
static gint compare_names(gconstpointer a,gconstpointer b) {return g_strcmp0(*(char *const *)a,*(char *const *)b);}
static void load_photo(GTask *task,gpointer source,gpointer data,GCancellable *cancel)
{
 PhotoJob *job=data;GPtrArray *files=g_ptr_array_new_with_free_func(g_free);GDir *dir=g_dir_open(job->folder,0,NULL);const char *name;
 (void)source;
 while(dir && files->len<2048 && !g_cancellable_is_cancelled(cancel) && (name=g_dir_read_name(dir))) {
  char *lower=g_ascii_strdown(name,-1);
  if(g_str_has_suffix(lower,".png")||g_str_has_suffix(lower,".jpg")||g_str_has_suffix(lower,".jpeg")||g_str_has_suffix(lower,".bmp")||g_str_has_suffix(lower,".gif")||g_str_has_suffix(lower,".webp"))
   g_ptr_array_add(files,g_build_filename(job->folder,name,NULL));
  g_free(lower);
 }
 if(dir)g_dir_close(dir);
 g_ptr_array_sort(files,compare_names);
 GdkPixbuf *photo=NULL;
 for(guint i=0;i<MIN(files->len,16u) && !photo && !g_cancellable_is_cancelled(cancel);i++)
  photo=gdk_pixbuf_new_from_file_at_scale(g_ptr_array_index(files,(job->index+i)%files->len),job->width,job->height,TRUE,NULL);
 g_ptr_array_unref(files);g_task_return_pointer(task,photo,photo?g_object_unref:NULL);
}
static void photo_ready(GObject *source,GAsyncResult *result,gpointer data)
{
 SaverView *s=g_object_get_data(source,"saver-view");PhotoJob *job=g_task_get_task_data(G_TASK(result));
 GdkPixbuf *photo=g_task_propagate_pointer(G_TASK(result),NULL);(void)data;
 s->loading=FALSE;
 if(!s->dead && job->generation==s->generation) {g_clear_object(&s->photo);s->photo=photo;gtk_widget_queue_draw(GTK_WIDGET(source));}
 else g_clear_object(&photo);
}
static gboolean view_draw(GtkWidget *widget,cairo_t *cr,gpointer data)
{
 SaverView *s=data;gint64 now=g_get_monotonic_time();
 if(s->config.mode==XP_SAVER_PICTURES && !s->loading && now>=s->next_photo) {
  PhotoJob *job=g_new0(PhotoJob,1);job->folder=g_strdup(s->config.pictures);job->index=s->photo_index++;
  job->generation=s->generation;job->width=MIN(1920,MAX(1,gtk_widget_get_allocated_width(widget)));job->height=MIN(1080,MAX(1,gtk_widget_get_allocated_height(widget)));
  GTask *task=g_task_new(widget,s->cancel,photo_ready,NULL);g_task_set_task_data(task,job,photo_job_free);
  s->loading=TRUE;s->next_photo=now+5*G_USEC_PER_SEC;g_task_run_in_thread(task,load_photo);g_object_unref(task);
 }
 draw_effect(s,cr,gtk_widget_get_allocated_width(widget),gtk_widget_get_allocated_height(widget),(now-s->started)/1000000.);return TRUE;
}
static gboolean animate_photo(gpointer data) {gtk_widget_queue_draw(GTK_WIDGET(data));return G_SOURCE_CONTINUE;}
static gboolean animate(GtkWidget *widget,GdkFrameClock *clock,gpointer data) {
 SaverView *s=data;gint64 now=gdk_frame_clock_get_frame_time(clock);
 if(now-s->last_frame>=32000){s->last_frame=now;gtk_widget_queue_draw(widget);}return G_SOURCE_CONTINUE;
}
static void view_map(GtkWidget *widget,gpointer data)
{
 SaverView *s=data;
 if(!s->timer && s->config.mode!=XP_SAVER_NONE && s->config.mode!=XP_SAVER_BLANK)
  s->timer=s->config.mode==XP_SAVER_PICTURES ? g_timeout_add(1000,animate_photo,widget) : gtk_widget_add_tick_callback(widget,animate,s,NULL);
}
static void view_unmap(GtkWidget *widget,gpointer data)
{ SaverView *s=data;(void)widget;if(s->timer){if(s->config.mode==XP_SAVER_PICTURES)g_source_remove(s->timer);else gtk_widget_remove_tick_callback(widget,s->timer);s->timer=0;s->last_frame=0;} }
static void view_destroy(GtkWidget *widget,gpointer data)
{SaverView *s=data;s->dead=TRUE;view_unmap(widget,s);g_cancellable_cancel(s->cancel);}
static void view_free(gpointer data)
{SaverView *s=data;xp_saver_config_clear(&s->config);g_clear_object(&s->photo);g_clear_object(&s->ascii);g_clear_object(&s->ascii_light);g_object_unref(s->cancel);if(s->pipes_cache)cairo_surface_destroy(s->pipes_cache);if(s->frame)cairo_surface_destroy(s->frame);g_free(s);}
void xp_saver_view_configure(GtkWidget *view,const XpSaverConfig *config)
{
 SaverView *s=g_object_get_data(G_OBJECT(view),"saver-view");view_unmap(view,s);xp_saver_config_clear(&s->config);xp_saver_config_copy(&s->config,config);
 if(s->pipes_cache){cairo_surface_destroy(s->pipes_cache);s->pipes_cache=NULL;}
 s->started=g_get_monotonic_time();s->generation++;s->next_photo=0;g_clear_object(&s->photo);
 if(gtk_widget_get_mapped(view))view_map(view,s);
 gtk_widget_queue_draw(view);
}
GtkWidget *xp_saver_view_new(const XpSaverConfig *config)
{
 GtkWidget *view=gtk_drawing_area_new();SaverView *s=g_new0(SaverView,1);s->cancel=g_cancellable_new();
 g_object_set_data_full(G_OBJECT(view),"saver-view",s,view_free);
 g_signal_connect(view,"draw",G_CALLBACK(view_draw),s);g_signal_connect(view,"map",G_CALLBACK(view_map),s);
 g_signal_connect(view,"unmap",G_CALLBACK(view_unmap),s);g_signal_connect(view,"destroy",G_CALLBACK(view_destroy),s);
 xp_saver_view_configure(view,config);return view;
}
