/* XP presentation for swaylock. Password authentication remains in upstream PAM. */
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wayland-client.h>
#include "swaylock.h"
#include "log.h"
#include "../../shared/session-art.h"

static void frame_done(void *data,struct wl_callback *callback,uint32_t time) {
 struct swaylock_surface *surface=data;(void)time;
 wl_callback_destroy(callback);surface->frame=NULL;render(surface);
}
static const struct wl_callback_listener frame_listener={.done=frame_done};
void render(struct swaylock_surface *surface) {
 struct swaylock_state *state=surface->state;
 if(!surface->width || !surface->height || !surface->dirty || surface->frame)return;
 static bool initialized;
 if(!initialized){luna_init();initialized=true;}
 int width=surface->width,height=surface->height,scale=surface->scale;
 int x=xp_session_account_x(width)+74,y=xp_session_account_y(height)+68;
 struct pool_buffer background={0};bool fresh=surface->last_buffer_width!=width*scale || surface->last_buffer_height!=height*scale;
 if(fresh) {
  if(!create_buffer(state->shm,&background,width*scale,height*scale,WL_SHM_FORMAT_ARGB8888))return;
  const char *name=g_get_real_name();if(!name || !*name || !strcmp(name,"Unknown"))name=g_get_user_name();
  XpSessionArt art={name,TRUE,FALSE,FALSE,TRUE,TRUE};
  cairo_scale(background.cairo,scale,scale);
  xp_session_draw_logon(background.cairo,width,height,&art);
  wl_surface_attach(surface->surface,background.buffer,0,0);
  wl_surface_damage_buffer(surface->surface,0,0,INT32_MAX,INT32_MAX);
 }
 /* Only the small password/status area is redrawn on keypresses. */
 struct pool_buffer *buffer=get_next_buffer(state->shm,surface->indicator_buffers,MAX(1,MIN(400,width-x))*scale,80*scale);
 if(!buffer){if(fresh)destroy_buffer(&background);return;}
 cairo_t *cr=buffer->cairo;cairo_identity_matrix(cr);
 cairo_set_operator(cr,CAIRO_OPERATOR_SOURCE);cairo_set_source_rgba(cr,0,0,0,0);cairo_paint(cr);cairo_set_operator(cr,CAIRO_OPERATOR_OVER);
 cairo_scale(cr,scale,scale);cairo_translate(cr,-x,-y);
 const char *status=state->auth_state==AUTH_STATE_VALIDATING ? "Checking your password..." : state->auth_state==AUTH_STATE_INVALID ? "The password is incorrect. Please try again." : state->xkb.caps_lock ? "Caps Lock is on." : "";
 /* Never access password contents; only a bounded masked length. */
 xp_session_password(cr,width,height,MIN(state->password.len,18u),status);
 wl_subsurface_set_position(surface->subsurface,x,y);
 wl_surface_set_buffer_scale(surface->child,scale);
 wl_surface_attach(surface->child,buffer->buffer,0,0);
 wl_surface_damage_buffer(surface->child,0,0,INT32_MAX,INT32_MAX);wl_surface_commit(surface->child);
 wl_surface_set_buffer_scale(surface->surface,scale);
 surface->dirty=false;surface->frame=wl_surface_frame(surface->surface);
 wl_callback_add_listener(surface->frame,&frame_listener,surface);
 wl_surface_commit(surface->surface);
 if(fresh){surface->last_buffer_width=width*scale;surface->last_buffer_height=height*scale;destroy_buffer(&background);}
}
