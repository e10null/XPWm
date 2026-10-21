/* Protocol resources may be destroyed in either order, including on disconnect. */
#define main dwl_original_main
#include "../compositor/dwl.c"
#undef main
#include <assert.h>

static void check_order(int decoration_first)
{
	struct wlr_xdg_toplevel_decoration_v1 decoration = {0};
	Client *c = ecalloc(1, sizeof(*c));
	struct wl_listener *listeners[] = {&c->destroy, &c->set_title,
		&c->fullscreen, &c->maximize, &c->minimize, &c->commit,
		&c->map, &c->unmap};
	size_t i;
	for (i = 0; i < LENGTH(listeners); i++)
		wl_list_init(&listeners[i]->link);
	wl_signal_init(&decoration.events.destroy);
	wl_signal_init(&decoration.events.request_mode);
	c->decoration = &decoration;
	LISTEN(&decoration.events.destroy, &c->destroy_decoration, destroydecoration);
	LISTEN(&decoration.events.request_mode, &c->set_decoration_mode, requestdecorationmode);
	if (decoration_first)
		wl_signal_emit_mutable(&decoration.events.destroy, &decoration);
	destroynotify(&c->destroy, NULL);
	assert(wl_list_empty(&decoration.events.destroy.listener_list));
	assert(wl_list_empty(&decoration.events.request_mode.listener_list));
	wl_signal_emit_mutable(&decoration.events.destroy, &decoration);
	wl_signal_emit_mutable(&decoration.events.request_mode, &decoration);
}

int main(void)
{
	check_order(0);
	check_order(1);
	puts("PASS: decoration and toplevel destruction in either order leaves no callbacks into freed clients");
	return 0;
}
