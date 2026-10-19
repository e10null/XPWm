/* Local Luna resources when available; recreated drawing remains the fallback. */
#include <stdlib.h>
#include <stdint.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <drm_fourcc.h>
#include <wlr/interfaces/wlr_buffer.h>
#include "decoration.h"
#include "../shared/luna.h"

struct CaptionBuffer {
	struct wlr_buffer base;
	cairo_surface_t *surface;
};

static void
destroy(struct wlr_buffer *base)
{
	struct CaptionBuffer *b = (struct CaptionBuffer *)base;
	cairo_surface_destroy(b->surface);
	free(b);
}

static bool
begin(struct wlr_buffer *base, uint32_t flags, void **data,
		uint32_t *format, size_t *stride)
{
	struct CaptionBuffer *b = (struct CaptionBuffer *)base;
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE)
		return false;
	*data = cairo_image_surface_get_data(b->surface);
	*format = DRM_FORMAT_ARGB8888;
	*stride = (size_t)cairo_image_surface_get_stride(b->surface);
	return true;
}

static void
end(struct wlr_buffer *base)
{
	(void)base;
}

static const struct wlr_buffer_impl impl = {
	.destroy = destroy, .begin_data_ptr_access = begin, .end_data_ptr_access = end,
};

struct wlr_buffer *
xp_caption_buffer(int width, const char *title, int active, int maximized,
		int hot, int pressed, int border, int can_maximize)
{
	struct CaptionBuffer *b = calloc(1, sizeof(*b));
	cairo_t *cr;
	cairo_pattern_t *gradient;
	PangoLayout *layout;
	PangoFontDescription *font;
	int i, x, themed, state, total_width = width;
	double shade;
	if (!b)
		return NULL;
	b->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, XP_CAPTION + border);
	if (cairo_surface_status(b->surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(b->surface);
		free(b);
		return NULL;
	}
	cr = cairo_create(b->surface);
	themed = luna_draw(cr, "caption", active ? 0 : 1, 0, 0, width, XP_CAPTION + border);
	if (!themed) {
		gradient = cairo_pattern_create_linear(0, 0, 0, XP_CAPTION);
		cairo_pattern_add_color_stop_rgb(gradient, 0, active ? .23 : .57, active ? .57 : .68, .96);
		cairo_pattern_add_color_stop_rgb(gradient, .22, active ? .02 : .44, active ? .31 : .56, active ? .94 : .82);
		cairo_pattern_add_color_stop_rgb(gradient, 1, active ? .02 : .48, active ? .24 : .59, active ? .76 : .84);
		cairo_set_source(cr, gradient);
		cairo_paint(cr);
		cairo_pattern_destroy(gradient);
	}
	cairo_translate(cr, border, border);
	width -= 2 * border;
	/* Use Explorer's generic application artwork until app icons are resolved. */
	if (!xp_icon(cr, 3, 16, 5, 6)) {
		cairo_set_source_rgb(cr, .96, .96, .91);
		cairo_rectangle(cr, 5, 6, 16, 15);
		cairo_fill(cr);
		cairo_set_source_rgb(cr, .1, .25, .65);
		cairo_rectangle(cr, 6, 7, 14, 4);
		cairo_fill(cr);
	}
	layout = pango_cairo_create_layout(cr);
	pango_cairo_context_set_resolution(pango_layout_get_context(layout), 96);
	font = pango_font_description_from_string(themed ? "Trebuchet MS Bold 10" : "Tahoma Bold 10");
	pango_layout_set_font_description(layout, font);
	pango_layout_set_text(layout, title ? title : "", -1);
	pango_layout_set_single_paragraph_mode(layout, TRUE);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	pango_layout_set_width(layout, MAX(0, width - 31 - 3 * (XP_BUTTON + XP_GAP)) * PANGO_SCALE);
	if (active && themed) {
		cairo_move_to(cr, 27, 6);
		cairo_set_source_rgb(cr, 10 / 255.0, 24 / 255.0, 131 / 255.0);
		pango_cairo_show_layout(cr, layout);
	}
	cairo_move_to(cr, 26, 5);
	if (active) cairo_set_source_rgb(cr, 1, 1, 1);
	else cairo_set_source_rgb(cr, 216 / 255.0, 228 / 255.0, 248 / 255.0);
	pango_cairo_show_layout(cr, layout);
	pango_font_description_free(font);
	g_object_unref(layout);
	for (i = 1; i <= 3; i++) {
		x = width - 3 - (4 - i) * (XP_BUTTON + XP_GAP);
		state = (active ? 0 : 4) + (i == 2 && !can_maximize ? 3 : hot == i ? (pressed == i ? 2 : 1) : 0);
		if (themed && luna_draw(cr, i == 3 ? "close-button" : "caption-button",
				state, x, 3, XP_BUTTON, XP_BUTTON)) {
			luna_draw(cr, i == 1 ? "min-glyph" : i == 3 ? "close-glyph" :
					maximized ? "restore-glyph" : "max-glyph", state, x + 4, 7, 13, 13);
			continue;
		}
		shade = pressed == i && hot == i ? .65 : hot == i ? 1.15 : 1;
		cairo_set_source_rgb(cr, (i == 3 ? .83 : .20) * shade,
				(i == 3 ? .24 : .43) * shade, (i == 3 ? .12 : .85) * shade);
		cairo_rectangle(cr, x + .5, 3.5, XP_BUTTON, XP_BUTTON);
		cairo_fill_preserve(cr);
		cairo_set_source_rgba(cr, 1, 1, 1, active ? .95 : .55);
		cairo_set_line_width(cr, 1);
		cairo_stroke(cr);
		cairo_set_line_width(cr, 2);
		if (i == 1) {
			cairo_move_to(cr, x + 5, 18); cairo_line_to(cr, x + 13, 18);
		} else if (i == 2) {
			if (maximized) {
				cairo_rectangle(cr, x + 8, 7, 8, 7);
				cairo_stroke(cr);
			}
			cairo_rectangle(cr, x + 5, maximized ? 10 : 7, 10, 10);
		} else {
			cairo_move_to(cr, x + 6, 8); cairo_line_to(cr, x + 15, 17);
			cairo_move_to(cr, x + 15, 8); cairo_line_to(cr, x + 6, 17);
		}
		cairo_stroke(cr);
	}
	cairo_destroy(cr);
	cairo_surface_flush(b->surface);
	wlr_buffer_init(&b->base, &impl, total_width, XP_CAPTION + border);
	return &b->base;
}

/* Thin frame buffers avoid allocating an entire transparent window overlay. */
struct wlr_buffer *
xp_frame_buffer(int width, int height, const char *role, int active)
{
	struct CaptionBuffer *b;
	cairo_t *cr;
	if (width < 1 || height < 1) return NULL;
	b = calloc(1, sizeof(*b));
	if (!b) return NULL;
	b->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status(b->surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(b->surface); free(b); return NULL;
	}
	cr = cairo_create(b->surface);
	if (!luna_draw(cr, role, active ? 0 : 1, 0, 0, width, height)) {
		cairo_set_source_rgb(cr, active ? .03 : .48, active ? .25 : .59, active ? .85 : .84);
		cairo_paint(cr);
	}
	cairo_destroy(cr); cairo_surface_flush(b->surface);
	wlr_buffer_init(&b->base, &impl, width, height);
	return &b->base;
}

/* XP's compact Alt+Tab list: original generic app artwork, selection frame and title. */
struct wlr_buffer *
xp_switcher_buffer(int width, int columns, int count, int selected, const char *title)
{
 struct CaptionBuffer *b = calloc(1, sizeof(*b));
 cairo_t *cr;
 PangoLayout *layout;
 PangoFontDescription *font;
 int slots = MIN(count, columns * 3), rows = (slots + columns - 1) / columns;
 int height = rows * 44 + 52, page = selected / (columns * 3) * (columns * 3), i, x, y;
 if (!b) return NULL;
 b->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
 if (cairo_surface_status(b->surface) != CAIRO_STATUS_SUCCESS) { cairo_surface_destroy(b->surface); free(b); return NULL; }
 cr = cairo_create(b->surface);
 cairo_set_source_rgb(cr, 236/255., 233/255., 216/255.); cairo_paint(cr);
 cairo_set_line_width(cr, 1);
 cairo_set_source_rgb(cr, 1, 1, 1); cairo_move_to(cr, 1, height-2); cairo_line_to(cr, 1, 1); cairo_line_to(cr, width-2, 1); cairo_stroke(cr);
 cairo_set_source_rgb(cr, .25, .25, .25); cairo_move_to(cr, 0, height-1); cairo_line_to(cr, width-1, height-1); cairo_line_to(cr, width-1, 0); cairo_stroke(cr);
 for (i = 0; i < slots && page+i < count; i++) {
  x = (width-columns*44)/2 + 6 + i % columns * 44; y = 10 + i / columns * 44;
  if (page+i == selected) {
   cairo_set_source_rgb(cr, .192, .416, .773); cairo_set_line_width(cr, 2);
   cairo_rectangle(cr, x-3, y-3, 40, 40); cairo_stroke(cr);
  }
  if (!xp_icon(cr, 3, 32, x, y)) {
   cairo_set_source_rgb(cr, 1, 1, 1); cairo_rectangle(cr, x, y, 32, 32); cairo_fill(cr);
   cairo_set_source_rgb(cr, .1, .25, .65); cairo_rectangle(cr, x+1, y+1, 30, 7); cairo_fill(cr);
  }
 }
 cairo_set_source_rgb(cr, .55, .55, .55); cairo_set_line_width(cr, 1);
 cairo_rectangle(cr, 9.5, height-32.5, width-19, 23); cairo_stroke(cr);
 layout = pango_cairo_create_layout(cr); pango_cairo_context_set_resolution(pango_layout_get_context(layout), 96);
 font = pango_font_description_from_string("Tahoma Bold 8"); pango_layout_set_font_description(layout, font);
 pango_layout_set_text(layout, title ? title : "", -1); pango_layout_set_single_paragraph_mode(layout, TRUE);
 pango_layout_set_width(layout, MAX(1, width-28)*PANGO_SCALE); pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
 pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER); cairo_set_source_rgb(cr, 0, 0, 0);
 cairo_move_to(cr, 14, height-28); pango_cairo_show_layout(cr, layout);
 pango_font_description_free(font); g_object_unref(layout); cairo_destroy(cr); cairo_surface_flush(b->surface);
 wlr_buffer_init(&b->base, &impl, width, height); return &b->base;
}

struct wlr_buffer *
xp_system_menu_buffer(int selected, unsigned int enabled)
{
 const char *labels[] = {"Restore", "Move", "Size", "Minimize", "Maximize", "Close"};
 struct CaptionBuffer *b = calloc(1, sizeof(*b));
 cairo_t *cr;
 PangoLayout *layout;
 PangoFontDescription *font;
 int i, y;
 if (!b) return NULL;
 b->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 180, 132);
 if (cairo_surface_status(b->surface) != CAIRO_STATUS_SUCCESS) { cairo_surface_destroy(b->surface); free(b); return NULL; }
 cr = cairo_create(b->surface); cairo_set_source_rgb(cr, 1, 1, 1); cairo_paint(cr);
 cairo_set_source_rgb(cr, .67, .66, .6); cairo_set_line_width(cr, 1); cairo_rectangle(cr, .5, .5, 179, 131); cairo_stroke(cr);
 layout = pango_cairo_create_layout(cr); pango_cairo_context_set_resolution(pango_layout_get_context(layout), 96);
 for (i = 0; i < 6; i++) {
  y = 3 + 20*i + (i == 5 ? 6 : 0);
  if (i == selected && (enabled & (1u << i))) {
   cairo_set_source_rgb(cr, 49/255., 106/255., 197/255.); cairo_rectangle(cr, 3, y, 174, 20); cairo_fill(cr);
   cairo_set_source_rgb(cr, 1, 1, 1);
  } else if (enabled & (1u << i)) cairo_set_source_rgb(cr, 0, 0, 0);
  else cairo_set_source_rgb(cr, .67, .66, .6);
  font = pango_font_description_from_string(i == 5 ? "Tahoma Bold 8" : "Tahoma 8");
  pango_layout_set_font_description(layout, font); pango_font_description_free(font);
  pango_layout_set_text(layout, labels[i], -1); cairo_move_to(cr, 24, y+3); pango_cairo_show_layout(cr, layout);
  if (i == 5) {
   font = pango_font_description_from_string("Tahoma 8"); pango_layout_set_font_description(layout, font); pango_font_description_free(font);
   pango_layout_set_text(layout, "Alt+F4", -1); cairo_move_to(cr, 125, y+3); pango_cairo_show_layout(cr, layout);
  }
 }
 cairo_set_source_rgb(cr, .67, .66, .6); cairo_rectangle(cr, 4, 105, 172, 1); cairo_fill(cr);
 g_object_unref(layout); cairo_destroy(cr); cairo_surface_flush(b->surface);
 wlr_buffer_init(&b->base, &impl, 180, 132); return &b->base;
}
