/* Standard Wayland task management; all actions are inhibited while locked. */
static void
foreignactivate(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, foreign_activate);
	struct wlr_foreign_toplevel_handle_v1_activated_event *event = data;
	if (locked || event->seat != seat || !c->mon) return;
	selmon = c->mon;
	if (!(c->tags & selmon->tagset[selmon->seltags]))
		view(&(Arg){.ui = c->tags});
	focusclient(c, 1);
}

static void
foreignminimize(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, foreign_minimize);
	struct wlr_foreign_toplevel_handle_v1_minimized_event *event = data;
	if (locked) return;
	if (event->minimized) xpminimize(c);
	else {
		c->minimized = 0;
#ifdef XWAYLAND
		if (client_is_x11(c)) wlr_xwayland_surface_set_minimized(c->surface.xwayland, 0);
#endif
		arrange(c->mon);
		foreignsync();
	}
}

static void
foreignmaximize(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, foreign_maximize);
	struct wlr_foreign_toplevel_handle_v1_maximized_event *event = data;
	if (!locked) { xpmaximize(c, event->maximized); foreignsync(); }
}

static void
foreignfullscreen(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, foreign_fullscreen);
	struct wlr_foreign_toplevel_handle_v1_fullscreen_event *event = data;
	if (!locked) setfullscreen(c, event->fullscreen);
}

static void
foreignclose(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, foreign_close);
	if (!locked) client_send_close(c);
}

static void
foreignsync(void)
{
	Client *c;
	const char *title, *appid;
	wl_list_for_each(c, &clients, link) {
		if (!c->foreign) continue;
		title = client_get_title(c); appid = client_get_appid(c);
		wlr_foreign_toplevel_handle_v1_set_title(c->foreign, title ? title : "Untitled");
		wlr_foreign_toplevel_handle_v1_set_app_id(c->foreign, appid ? appid : "");
		wlr_foreign_toplevel_handle_v1_set_activated(c->foreign, c->caption_active && !c->minimized);
		wlr_foreign_toplevel_handle_v1_set_minimized(c->foreign, c->minimized);
		wlr_foreign_toplevel_handle_v1_set_maximized(c->foreign, c->maximized);
		wlr_foreign_toplevel_handle_v1_set_fullscreen(c->foreign, c->isfullscreen);
	}
}

static void
foreigncreate(Client *c)
{
	c->foreign = wlr_foreign_toplevel_handle_v1_create(foreign_manager);
	if (!c->foreign) return;
	LISTEN(&c->foreign->events.request_activate, &c->foreign_activate, foreignactivate);
	LISTEN(&c->foreign->events.request_minimize, &c->foreign_minimize, foreignminimize);
	LISTEN(&c->foreign->events.request_maximize, &c->foreign_maximize, foreignmaximize);
	LISTEN(&c->foreign->events.request_fullscreen, &c->foreign_fullscreen, foreignfullscreen);
	LISTEN(&c->foreign->events.request_close, &c->foreign_close, foreignclose);
}

static void
foreigndestroy(Client *c)
{
	if (!c->foreign) return;
	wl_list_remove(&c->foreign_activate.link);
	wl_list_remove(&c->foreign_minimize.link);
	wl_list_remove(&c->foreign_maximize.link);
	wl_list_remove(&c->foreign_fullscreen.link);
	wl_list_remove(&c->foreign_close.link);
	wlr_foreign_toplevel_handle_v1_destroy(c->foreign);
	c->foreign = NULL;
}
