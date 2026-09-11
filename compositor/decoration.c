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
		int hot, int pressed)
{
	struct CaptionBuffer *b = calloc(1, sizeof(*b));
	cairo_t *cr;
	cairo_pattern_t *gradient;
	PangoLayout *layout;
	PangoFontDescription *font;
	int i, x, themed, state;
	double shade;
	if (!b)
		return NULL;
	b->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, XP_CAPTION);
	if (cairo_surface_status(b->surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(b->surface);
		free(b);
		return NULL;
	}
	cr = cairo_create(b->surface);
	themed = luna_draw(cr, "caption", active ? 0 : 1, 0, 0, width, XP_CAPTION);
	if (!themed) {
	gradient = cairo_pattern_create_linear(0, 0, 0, XP_CAPTION);
	cairo_pattern_add_color_stop_rgb(gradient, 0, active ? .23 : .57, active ? .57 : .68, .96);
	cairo_pattern_add_color_stop_rgb(gradient, .22, active ? .02 : .44, active ? .31 : .56, active ? .94 : .82);
	cairo_pattern_add_color_stop_rgb(gradient, 1, active ? .02 : .48, active ? .24 : .59, active ? .76 : .84);
	cairo_set_source(cr, gradient);
	cairo_paint(cr);
	cairo_pattern_destroy(gradient);
	}
	/* Generic window icon until per-application icons are integrated. */
	cairo_set_source_rgb(cr, .96, .96, .91);
	cairo_rectangle(cr, 5, 6, 16, 15);
	cairo_fill(cr);
	cairo_set_source_rgb(cr, .1, .25, .65);
	cairo_rectangle(cr, 6, 7, 14, 4);
	cairo_fill(cr);
	layout = pango_cairo_create_layout(cr);
	pango_cairo_context_set_resolution(pango_layout_get_context(layout), 96);
	font = pango_font_description_from_string(themed ? "Trebuchet MS Bold 10" : "Tahoma Bold 10");
	pango_layout_set_font_description(layout, font);
	pango_layout_set_text(layout, title ? title : "", -1);
	pango_layout_set_single_paragraph_mode(layout, TRUE);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	pango_layout_set_width(layout, (width - 31 - 3 * (XP_BUTTON + XP_GAP)) * PANGO_SCALE);
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
		state = (active ? 0 : 4) + (hot == i ? (pressed == i ? 2 : 1) : 0);
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
	wlr_buffer_init(&b->base, &impl, width, XP_CAPTION);
	return &b->base;
}
