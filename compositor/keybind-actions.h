/* Local actions used by the imported Hyprland shortcuts. */
static void
xpworkspace(const Arg *arg)
{
	unsigned int i, current;
	if (!selmon)
		return;
	current = selmon->tagset[selmon->seltags] & 1023u;
	for (i = 0; i < 10 && !(current & (1u << i)); i++)
		;
	i = (i + 10 + arg->i) % 10;
	view(&(Arg){.ui = 1u << i});
}

static void
xptagfollow(const Arg *arg)
{
	tag(arg);
	view(arg);
}

static void
xpallfloat(const Arg *arg)
{
	Client *c;
	int to_floating;
	if (!selmon) return;
	to_floating = selmon->lt[selmon->sellt]->arrange != NULL;
	if (!to_floating) {
		system_stop_move(1);
		if (cursor_mode == CurMove || cursor_mode == CurResize) {
			cursor_mode = CurNormal; grabc = NULL; resize_edges = 0;
			wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
		}
	}
	wl_list_for_each(c, &clients, link) {
		if (c->mon != selmon || c->isfullscreen ||
				client_get_parent(c) || client_is_float_type(c)) continue;
		if (!to_floating) {
			c->floatingmaximized = c->maximized;
			if (c->maximized) xpmaximize(c, 0);
			c->floatinggeom = c->geom;
			c->floatinggeom_valid = 1;
			c->isfloating = 0;
		}
	}
	setlayout(&(Arg){.v = &layouts[to_floating ? 1 : 0]});
	if (to_floating) {
		wl_list_for_each(c, &clients, link) {
			if (c->mon == selmon && !c->isfullscreen && !c->maximized && c->floatinggeom_valid) {
				resize(c, c->floatinggeom, 0);
				c->floatinggeom_valid = 0;
				if (c->floatingmaximized) xpmaximize(c, 1);
				c->floatingmaximized = 0;
			}
		}
	}
}

static void
xpgeometry(const Arg *arg)
{
	Client *c = focustop(selmon);
	struct wlr_box box;
	if (!c || c->isfullscreen || !client_manual_geometry(c))
		return;
	if (c->maximized)
		xpmaximize(c, 0);
	box = c->geom;
	/* Floating keyboard moves use a documented 20-logical-pixel step. */
	switch (arg->i) {
	case 0: box.x -= 20; break;
	case 1: box.x += 20; break;
	case 2: box.y -= 20; break;
	case 3: box.y += 20; break;
	case 4: box.width -= 20; break;
	case 5: box.width += 20; break;
	case 6: box.height -= 20; break;
	case 7: box.height += 20; break;
	case 8:
		box.x = selmon->w.x + (selmon->w.width - box.width) / 2;
		box.y = selmon->w.y + (selmon->w.height - box.height) / 2;
		break;
	}
	setfloating(c, 1);
	resize(c, box, 1);
}

static void
xpfocus(const Arg *arg)
{
	Client *c, *best = NULL, *sel = focustop(selmon);
	double dx, dy, primary, cross, score, bestscore = HUGE_VAL;
	if (!sel || sel->isfullscreen)
		return;
	wl_list_for_each(c, &clients, link) {
		if (c == sel || !VISIBLEON(c, selmon))
			continue;
		dx = c->geom.x + c->geom.width / 2.0 - sel->geom.x - sel->geom.width / 2.0;
		dy = c->geom.y + c->geom.height / 2.0 - sel->geom.y - sel->geom.height / 2.0;
		primary = arg->i < 2 ? dx : dy;
		cross = arg->i < 2 ? dy : dx;
		if (arg->i == 0 || arg->i == 2)
			primary = -primary;
		if (primary <= 0)
			continue;
		score = primary * primary + cross * cross;
		if (score < bestscore) {
			bestscore = score;
			best = c;
		}
	}
	if (best)
		focusclient(best, 1);
}
