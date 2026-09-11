/* Window state and caption input stay inside dwl's Client/scene lifecycle. */
static void
drawcaption(Client *c)
{
	struct wlr_buffer *buffer;
	if (!c->caption)
		return;
	wlr_scene_node_set_enabled(&c->caption->node, !c->isfullscreen);
	if (c->isfullscreen)
		return;
	buffer = xp_caption_buffer(MAX(XP_MIN_WIDTH, c->geom.width - 2 * (int)c->bw),
			client_get_title(c), c->caption_active, c->maximized,
			c->caption_hot, caption_pressed == c ? pressed_button : 0);
	if (!buffer)
		return;
	wlr_scene_buffer_set_buffer(c->caption, buffer);
	wlr_buffer_drop(buffer);
	wlr_scene_node_set_position(&c->caption->node, c->bw, c->bw);
}

static void
xpmaximize(Client *c, int enabled)
{
	if (!c || !c->mon || !client_surface(c)->mapped || c->isfullscreen
			|| c->maximized == enabled)
		return;
	if (enabled) {
		c->normalgeom = c->geom;
		c->normalfloating = c->isfloating;
		c->maximized = 1;
		setfloating(c, 1);
		resize(c, c->mon->w, 0);
	} else {
		c->maximized = 0;
		resize(c, c->normalgeom, 0);
		setfloating(c, c->normalfloating);
	}
#ifdef XWAYLAND
	if (client_is_x11(c))
		wlr_xwayland_surface_set_maximized(c->surface.xwayland, enabled);
	else
#endif
		wlr_xdg_toplevel_set_maximized(c->surface.xdg->toplevel, enabled);
	drawcaption(c);
	foreignsync();
}

static void
xpminimize(Client *c)
{
	if (!c || !c->mon || client_is_unmanaged(c))
		return;
	c->minimized = 1;
#ifdef XWAYLAND
	if (client_is_x11(c))
		wlr_xwayland_surface_set_minimized(c->surface.xwayland, 1);
#endif
	wlr_scene_node_set_enabled(&c->scene->node, 0);
	focusclient(focustop(c->mon), 1);
	arrange(c->mon);
	printstatus();
}

static int
captionbutton(Client *c, double x, double y)
{
	int i, left, width = c->geom.width - 2 * (int)c->bw;
	x -= c->geom.x + c->bw;
	y -= c->geom.y + c->bw;
	if (y < 3 || y >= 3 + XP_BUTTON)
		return 0;
	for (i = 1; i <= 3; i++) {
		left = width - 3 - (4 - i) * (XP_BUTTON + XP_GAP);
		if (x >= left && x < left + XP_BUTTON)
			return i;
	}
	return 0;
}

static int
decorationpress(Client *c, struct wlr_pointer_button_event *event)
{
	double x = cursor->x - c->geom.x, y = cursor->y - c->geom.y;
	unsigned int edges = 0;
	if (client_is_unmanaged(c) || c->isfullscreen || event->button != BTN_LEFT)
		return 0;
	if (x < c->bw) edges |= WLR_EDGE_LEFT;
	if (x >= c->geom.width - c->bw) edges |= WLR_EDGE_RIGHT;
	if (y < c->bw) edges |= WLR_EDGE_TOP;
	if (y >= c->geom.height - c->bw) edges |= WLR_EDGE_BOTTOM;
	if (edges) {
		if (c->maximized)
			return 1;
		setfloating(c, 1);
		grabc = c;
		resize_edges = edges;
		resize_origin = c->geom;
		resize_x = cursor->x;
		resize_y = cursor->y;
		cursor_mode = CurResize;
		wlr_cursor_set_xcursor(cursor, cursor_mgr,
				edges == WLR_EDGE_LEFT || edges == WLR_EDGE_RIGHT ? "ew-resize" :
				edges == WLR_EDGE_TOP || edges == WLR_EDGE_BOTTOM ? "ns-resize" :
				edges == (WLR_EDGE_TOP | WLR_EDGE_LEFT) ||
				edges == (WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT) ? "nwse-resize" : "nesw-resize");
		return 1;
	}
	if (y >= c->bw + XP_CAPTION)
		return 0;
	pressed_button = captionbutton(c, cursor->x, cursor->y);
	if (pressed_button) {
		caption_pressed = c;
		c->caption_hot = pressed_button;
		drawcaption(c);
		return 1;
	}
	/* Provisional 500ms double-click threshold; XP mouse settings are pending. */
	if (caption_clicked == c && event->time_msec - caption_click_time < 500) {
		caption_clicked = NULL;
		xpmaximize(c, !c->maximized);
		return 1;
	}
	caption_clicked = c;
	caption_click_time = event->time_msec;
	caption_click_x = cursor->x;
	caption_click_y = cursor->y;
	if (!c->maximized)
		moveresize(&(Arg){.ui = CurMove});
	return 1;
}

static int
decorationrelease(struct wlr_pointer_button_event *event)
{
	Client *c = caption_pressed;
	int button = pressed_button;
	if (!c || event->button != BTN_LEFT)
		return 0;
	caption_pressed = NULL;
	cursor_mode = CurNormal;
	drawcaption(c);
	if (locked || captionbutton(c, cursor->x, cursor->y) != button)
		return 1;
	if (button == 1)
		xpminimize(c);
	else if (button == 2)
		xpmaximize(c, !c->maximized);
	else if (button == 3)
		client_send_close(c);
	return 1;
}

static void
decorationmotion(void)
{
	Client *c = NULL;
	int hot;
	if (caption_clicked && (fabs(cursor->x - caption_click_x) > 4 ||
			fabs(cursor->y - caption_click_y) > 4))
		caption_clicked = NULL;
	xytonode(cursor->x, cursor->y, NULL, &c, NULL, NULL, NULL);
	if (c && (client_is_unmanaged(c) || c->isfullscreen))
		c = NULL;
	if (caption_hover && caption_hover != c) {
		caption_hover->caption_hot = 0;
		drawcaption(caption_hover);
	}
	caption_hover = c;
	if (c) {
		hot = captionbutton(c, cursor->x, cursor->y);
		if (hot != c->caption_hot) {
			c->caption_hot = hot;
			drawcaption(c);
		}
	}
}
