/* Shared XP LogonUI artwork, used by the Welcome screen and secure locker. */
#ifndef XPWM_SESSION_ART_H
#define XPWM_SESSION_ART_H
#include <glib.h>
#include <pango/pangocairo.h>
#include "luna.h"
typedef struct {const char *username; gboolean selected,hovered,accepting,password_prompt,locked;} XpSessionArt;
static inline void xp_session_text(cairo_t *cr, const char *value, double x, double y, double width,
		const char *font, PangoAlignment alignment)
{
	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoFontDescription *description = pango_font_description_from_string(font);
	pango_cairo_context_set_resolution(pango_layout_get_context(layout), 96);
 cairo_font_options_t *options=cairo_font_options_create();
 cairo_font_options_set_antialias(options,CAIRO_ANTIALIAS_GRAY);
 cairo_font_options_set_hint_style(options,CAIRO_HINT_STYLE_FULL);
 pango_cairo_context_set_font_options(pango_layout_get_context(layout),options);
 cairo_font_options_destroy(options);
	pango_layout_set_font_description(layout, description);
	pango_layout_set_text(layout, value ? value : "", -1);
	pango_layout_set_width(layout, MAX(1, (int)width) * PANGO_SCALE);
	pango_layout_set_alignment(layout, alignment);
	pango_layout_set_single_paragraph_mode(layout, TRUE);
	cairo_new_path(cr); cairo_move_to(cr, x, y); pango_cairo_show_layout(cr, layout);
	pango_font_description_free(description); g_object_unref(layout);
}

static inline void xp_session_xp_logo(cairo_t *cr, double x, double y, double scale)
{
	static const double colors[4][3] = {
		{.88, .12, .10}, {.15, .70, .18}, {.10, .35, .85}, {.96, .72, .08}
	};
	cairo_save(cr); cairo_translate(cr, x, y); cairo_scale(cr, scale, scale);
	for (int i = 0; i < 4; i++) {
		double ox = (i & 1) * 18, oy = (i / 2) * 18;
		cairo_set_source_rgb(cr, colors[i][0], colors[i][1], colors[i][2]);
		cairo_move_to(cr, ox, oy + (i / 2 ? 1 : 0));
		cairo_line_to(cr, ox + 16, oy); cairo_line_to(cr, ox + 16, oy + 16);
		cairo_line_to(cr, ox, oy + (i / 2 ? 15 : 16)); cairo_close_path(cr); cairo_fill(cr);
	}
	cairo_restore(cr);
}

/* Geometry and colors follow LogonUI's UIFILE 1000, not the Luna desktop.
 * Top band: 80rp. Footer: 20rp padding + two instruction lines + divider. */
static inline int xp_session_content_middle(int height) { return (80 + height - 74) / 2; }
static inline int xp_session_account_x(int width) { return width / 2 + 26; }
static inline int xp_session_account_y(int height) { return xp_session_content_middle(height) - 40; }

static inline void xp_session_background(cairo_t *cr, int width, int height)
{
	cairo_pattern_t *gradient;
	cairo_set_source_rgb(cr, 90/255., 126/255., 220/255.); cairo_paint(cr);
	cairo_save(cr); cairo_rectangle(cr, 0, 80, width, MAX(1, height-154)); cairo_clip(cr);
	xp_bitmap(cr, "xp-bitmaps/logonui.exe-BITMAP-100-1033.png", 0, 80);
	cairo_restore(cr);
	cairo_set_source_rgb(cr, 0, 48/255., 156/255.);
	cairo_rectangle(cr, 0, 0, width, 80); cairo_fill(cr);
	gradient = cairo_pattern_create_linear(0, 0, width, 0);
	cairo_pattern_add_color_stop_rgb(gradient, 0, 57/255., 52/255., 173/255.);
	cairo_pattern_add_color_stop_rgb(gradient, 1, 0, 48/255., 156/255.);
	cairo_set_source(cr, gradient); cairo_rectangle(cr, 0, height-74, width, 74); cairo_fill(cr);
	cairo_pattern_destroy(gradient);
	cairo_save(cr); cairo_scale(cr, width/800., 1);
	xp_bitmap(cr, "xp-bitmaps/logonui.exe-BITMAP-125-1033.png", 0, 78);
	xp_bitmap(cr, "xp-bitmaps/logonui.exe-BITMAP-126-1033.png", 0, height-74);
	cairo_restore(cr);
}

static inline void xp_session_fallback_user(cairo_t *cr, double x, double y)
{
	cairo_set_source_rgb(cr, .78, .86, .96); cairo_arc(cr, x + 24, y + 17, 9, 0, 2 * G_PI); cairo_fill(cr);
	cairo_set_source_rgb(cr, .35, .55, .78); cairo_arc(cr, x + 24, y + 43, 18, G_PI, 2 * G_PI); cairo_fill(cr);
}

static inline void xp_session_product(cairo_t *cr, double x, double y)
{
	if (xp_bitmap(cr, "xp-bitmaps/logonui.exe-BITMAP-123-1033.png", x, y)) return;
	xp_session_xp_logo(cr, x+53, y, 1.6);
	cairo_set_source_rgb(cr, 1, 1, 1);
	xp_session_text(cr, "Microsoft", x, y+52, 137, "Arial 7", PANGO_ALIGN_LEFT);
	xp_session_text(cr, "Windows", x, y+61, 137, "Arial Bold 20", PANGO_ALIGN_LEFT);
}

static inline void xp_session_user_tile(cairo_t *cr, int width, int height, const XpSessionArt *art)
{
	int x = xp_session_account_x(width), y = xp_session_account_y(height);
	if (art->selected) xp_bitmap(cr, "xp-bitmaps/logonui.exe-BITMAP-112-1033.png", x, y);
	if (!xp_bitmap(cr, (art->selected || art->hovered) ? "xp-bitmaps/logonui.exe-BITMAP-119-1033.png" :
			"xp-bitmaps/logonui.exe-BITMAP-113-1033.png", x+7, y+7)) {
		cairo_set_source_rgb(cr, 1, 1, .9); cairo_rectangle(cr, x+7, y+7, 58, 58); cairo_fill(cr);
	}
	if (!xp_bitmap(cr, "xp-bitmaps/logonui.exe-BITMAP-114-1033.png", x+12, y+12))
		xp_session_fallback_user(cr, x+12, y+12);
	cairo_set_source_rgb(cr, 239/255., 247/255., 1);
	xp_session_text(cr, art->username, x+72, y+18, MAX(1, width-x-90), "Tahoma 14", PANGO_ALIGN_LEFT);
	if (art->selected && art->password_prompt)
		xp_session_text(cr, "Type your password", x+74, y+49, 230, "Arial 8", PANGO_ALIGN_LEFT);
}

static inline void xp_session_draw_logon(cairo_t *cr, int width, int height, const XpSessionArt *art)
{
	int middle = xp_session_content_middle(height), split = width/2;
	xp_session_background(cr, width, height);
	if (art->accepting) {
		cairo_set_source_rgb(cr, 49/255., 81/255., 181/255.);
		xp_session_text(cr, "welcome", split-264, middle-30+3, 244, "Arial Bold Italic 36", PANGO_ALIGN_RIGHT);
		cairo_set_source_rgb(cr, 239/255., 247/255., 1);
		xp_session_text(cr, "welcome", split-266, middle-30, 244, "Arial Bold Italic 36", PANGO_ALIGN_RIGHT);
		return;
	}
	xp_session_product(cr, split-157, middle-70);
	cairo_set_source_rgb(cr, 239/255., 247/255., 1);
	xp_session_text(cr, art->locked ? "This computer is locked" : "To begin, click your user name", MAX(0, split-384), middle+36,
		MIN(344, split-16), "Arial 14", PANGO_ALIGN_RIGHT);
	if (!xp_bitmap(cr, "xp-bitmaps/logonui.exe-BITMAP-124-1033.png", split, middle-170)) {
		cairo_set_source_rgba(cr, 1, 1, 1, .3); cairo_rectangle(cr, split, middle-170, 1, 340); cairo_fill(cr);
	}
	xp_session_user_tile(cr, width, height, art);
	cairo_set_source_rgb(cr, 1, 1, 1);
	if (width >= 720) {
		xp_session_text(cr, art->locked ? "Only this user can unlock this computer." : "After you log on, you can add or change accounts.", width-350, height-52,
			325, "Tahoma 9", PANGO_ALIGN_LEFT);
		xp_session_text(cr, art->locked ? "Type your password and press Enter." : "Just go to Control Panel and click User Accounts.", width-350, height-37,
			325, "Tahoma 9", PANGO_ALIGN_LEFT);
	}
}

static inline void xp_session_draw_logoff(cairo_t *cr, int width, int height)
{
	int middle = xp_session_content_middle(height);
	xp_session_background(cr, width, height);
	xp_session_product(cr, width/2-69, middle-77);
	cairo_set_source_rgb(cr, 239/255., 247/255., 1);
	xp_session_text(cr, "Logging off...", width/2-190, middle+30, 380, "Tahoma 10", PANGO_ALIGN_CENTER);
}


static inline void xp_session_password(cairo_t *cr, int width, int height, unsigned count, const char *status)
{
 int x=xp_session_account_x(width)+74,y=xp_session_account_y(height)+68;
 cairo_set_source_rgb(cr,1,1,1);cairo_rectangle(cr,x,y,160,26);cairo_fill_preserve(cr);
 cairo_set_source_rgb(cr,.45,.57,.90);cairo_set_line_width(cr,1);cairo_stroke(cr);
 cairo_save(cr);cairo_rectangle(cr,x+4,y+2,152,22);cairo_clip(cr);
 cairo_set_source_rgb(cr,0,0,0);
 for(unsigned i=0;i<MIN(count,18u);i++){cairo_arc(cr,x+8+i*8,y+13,2,0,2*G_PI);cairo_fill(cr);}
 cairo_restore(cr);
 if(!xp_bitmap(cr,"xp-bitmaps/logonui.exe-BITMAP-103-1033.png",x+168,y)) {
  cairo_set_source_rgb(cr,.2,.65,.15);cairo_rectangle(cr,x+168,y,26,26);cairo_fill(cr);
  cairo_set_source_rgb(cr,1,1,1);xp_session_text(cr,"→",x+168,y,26,"Arial Bold 14",PANGO_ALIGN_CENTER);
 }
 cairo_set_source_rgb(cr,1,1,1);xp_session_text(cr,status,x,y+34,MAX(1,width-x-12),"Tahoma 8",PANGO_ALIGN_LEFT);
}
#endif
