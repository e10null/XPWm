/* Native XP tray balloons implementing the freedesktop notification service. */
#include <gtk/gtk.h>
#include <gtk-layer-shell.h>
#include "notifications.h"
#include "../shared/sounds.h"

#define SERVICE "org.freedesktop.Notifications"
#define OBJECT "/org/freedesktop/Notifications"

typedef struct {
	guint id;
	char *summary, *body;
	char **actions;
	int timeout;
	guchar urgency;
 gboolean silent, sounded, battery;
 int power_event;
} Notification;

static GDBusConnection *connection;
static GDBusNodeInfo *introspection;
static guint owner, registration, timer, next_id;
static GQueue pending = G_QUEUE_INIT;
static Notification *active;
static GtkWidget *balloon, *content;

static const char xml[] =
	"<node><interface name='org.freedesktop.Notifications'>"
	"<method name='GetCapabilities'><arg type='as' direction='out'/></method>"
	"<method name='GetServerInformation'><arg type='s' direction='out'/><arg type='s' direction='out'/><arg type='s' direction='out'/><arg type='s' direction='out'/></method>"
	"<method name='Notify'><arg type='s' direction='in'/><arg type='u' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='as' direction='in'/><arg type='a{sv}' direction='in'/><arg type='i' direction='in'/><arg type='u' direction='out'/></method>"
	"<method name='CloseNotification'><arg type='u' direction='in'/></method>"
	"<signal name='NotificationClosed'><arg type='u'/><arg type='u'/></signal>"
	"<signal name='ActionInvoked'><arg type='u'/><arg type='s'/></signal>"
	"</interface></node>";

static void show_next(void);

static void free_notification(Notification *n)
{
	g_free(n->summary); g_free(n->body); g_strfreev(n->actions); g_free(n);
}

static Notification *find_notification(guint id)
{
	if (active && active->id == id) return active;
	for (GList *l = pending.head; l; l = l->next)
		if (((Notification *)l->data)->id == id) return l->data;
	return NULL;
}

static void close_notification(Notification *n, guint reason)
{
	if (!n) return;
	if (n == active) {
		if (timer) { g_source_remove(timer); timer = 0; }
		gtk_widget_hide(balloon);
		active = NULL;
	} else g_queue_remove(&pending, n);
	g_dbus_connection_emit_signal(connection, NULL, OBJECT, SERVICE,
		"NotificationClosed", g_variant_new("(uu)", n->id, reason), NULL);
	free_notification(n);
	show_next();
}

static gboolean expire(gpointer data)
{
	timer = 0;
	close_notification(active, 1);
	return G_SOURCE_REMOVE;
}

static void dismiss(GtkButton *button, gpointer data) { close_notification(active, 2); }

static void invoke(GtkButton *button, gpointer data)
{
	if (!active) return;
	g_dbus_connection_emit_signal(connection, NULL, OBJECT, SERVICE,
		"ActionInvoked", g_variant_new("(us)", active->id, (const char *)data), NULL);
	close_notification(active, 2);
}

static gboolean clicked(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	if (event->button != 1 || !active) return FALSE;
	for (guint i = 0; active->actions && active->actions[i] && active->actions[i+1]; i += 2)
		if (!strcmp(active->actions[i], "default")) { invoke(NULL, "default"); return TRUE; }
	close_notification(active, 2);
	return TRUE;
}

static gboolean paint(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	double w = gtk_widget_get_allocated_width(widget)-3, bottom = gtk_widget_get_allocated_height(widget)-21;
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0); cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	/* Rounded tooltip body and a single continuous pointer toward the tray. */
	cairo_new_path(cr);
	cairo_move_to(cr, 8, 1);
	cairo_line_to(cr, w-8, 1); cairo_curve_to(cr, w-2, 1, w, 3, w, 9);
	cairo_line_to(cr, w, bottom-8); cairo_curve_to(cr, w, bottom-2, w-2, bottom, w-8, bottom);
	cairo_line_to(cr, w-33, bottom); cairo_line_to(cr, w-25, bottom+18);
	cairo_line_to(cr, w-53, bottom); cairo_line_to(cr, 8, bottom);
	cairo_curve_to(cr, 2, bottom, 1, bottom-2, 1, bottom-8);
	cairo_line_to(cr, 1, 9); cairo_curve_to(cr, 1, 3, 2, 1, 8, 1); cairo_close_path(cr);
	cairo_set_source_rgb(cr, 1, 1, 225/255.); cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, .18, .18, .18); cairo_set_line_width(cr, 1); cairo_stroke(cr);
	return FALSE;
}

static gboolean paint_icon(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	int urgency = active ? active->urgency : 1;
	cairo_set_source_rgb(cr, urgency == 2 ? .8 : .1, .25, urgency == 2 ? .1 : .75);
	cairo_arc(cr, 9, 9, 8, 0, 2*G_PI); cairo_fill(cr);
	cairo_set_source_rgb(cr, 1, 1, 1); cairo_set_line_width(cr, 2);
	if (urgency == 2) {
		cairo_move_to(cr, 6, 6); cairo_line_to(cr, 12, 12);
		cairo_move_to(cr, 12, 6); cairo_line_to(cr, 6, 12);
	} else {
		cairo_move_to(cr, 9, 8); cairo_line_to(cr, 9, 13);
		cairo_move_to(cr, 9, 4); cairo_line_to(cr, 9, 6);
	}
	cairo_stroke(cr); return FALSE;
}

static GtkWidget *label(const char *text, int lines)
{
	GtkWidget *widget = gtk_label_new(text);
	gtk_label_set_xalign(GTK_LABEL(widget), 0);
	gtk_label_set_line_wrap(GTK_LABEL(widget), TRUE);
	gtk_label_set_line_wrap_mode(GTK_LABEL(widget), PANGO_WRAP_WORD_CHAR);
	gtk_label_set_max_width_chars(GTK_LABEL(widget), 42);
	gtk_label_set_lines(GTK_LABEL(widget), lines);
	gtk_label_set_ellipsize(GTK_LABEL(widget), PANGO_ELLIPSIZE_END);
	return widget;
}

static void render(void)
{
	if (timer) { g_source_remove(timer); timer = 0; }
	if (!balloon) {
		balloon = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_widget_set_name(balloon, "xpwm-balloon");
		gtk_window_set_title(GTK_WINDOW(balloon), "XP Notification");
		gtk_widget_set_app_paintable(balloon, TRUE);
		GdkVisual *visual = gdk_screen_get_rgba_visual(gtk_widget_get_screen(balloon));
		if (visual) gtk_widget_set_visual(balloon, visual);
		gtk_layer_init_for_window(GTK_WINDOW(balloon));
		gtk_layer_set_namespace(GTK_WINDOW(balloon), "xpwm-balloon");
		gtk_layer_set_layer(GTK_WINDOW(balloon), GTK_LAYER_SHELL_LAYER_TOP);
		gtk_layer_set_keyboard_mode(GTK_WINDOW(balloon), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
		gtk_layer_set_anchor(GTK_WINDOW(balloon), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
		gtk_layer_set_anchor(GTK_WINDOW(balloon), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
		gtk_layer_set_margin(GTK_WINDOW(balloon), GTK_LAYER_SHELL_EDGE_BOTTOM, 30);
		gtk_layer_set_margin(GTK_WINDOW(balloon), GTK_LAYER_SHELL_EDGE_RIGHT, 12);
		gtk_layer_set_exclusive_zone(GTK_WINDOW(balloon), -1);
		gtk_widget_set_size_request(balloon, 320, -1);
		g_signal_connect(balloon, "draw", G_CALLBACK(paint), NULL);
		gtk_widget_add_events(balloon, GDK_BUTTON_PRESS_MASK);
		g_signal_connect(balloon, "button-press-event", G_CALLBACK(clicked), NULL);
		GtkCssProvider *css = gtk_css_provider_new();
		gtk_css_provider_load_from_data(css,
			"#xpwm-balloon, #xpwm-balloon * { font:8pt Tahoma; color:#000; text-shadow:none; box-shadow:none; }"
			"#xpwm-balloon { background:transparent; }"
			"#xpwm-balloon .balloon-title { font-weight:bold; }"
			"#xpwm-balloon button { min-width:16px; min-height:16px; padding:0 3px; margin:0; border:1px solid #aca899; border-radius:2px; background:#ffffe1; }"
			"#xpwm-balloon button:hover { background:#fff; border-color:#316ac5; }", -1, NULL);
		gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(balloon), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER+10);
		g_object_unref(css);
	}
	if (content) gtk_widget_destroy(content);
	content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);
	gtk_widget_set_margin_start(content, 11); gtk_widget_set_margin_end(content, 14);
	gtk_widget_set_margin_top(content, 9); gtk_widget_set_margin_bottom(content, 30);
	gtk_container_add(GTK_CONTAINER(balloon), content);
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7);
	GtkWidget *icon = gtk_drawing_area_new(); gtk_widget_set_size_request(icon, 18, 18);
	g_signal_connect(icon, "draw", G_CALLBACK(paint_icon), NULL);
	gtk_box_pack_start(GTK_BOX(row), icon, FALSE, FALSE, 0);
	GtkWidget *title = label(active->summary, 2);
	gtk_style_context_add_class(gtk_widget_get_style_context(title), "balloon-title");
	gtk_box_pack_start(GTK_BOX(row), title, TRUE, TRUE, 0);
	GtkWidget *close = gtk_button_new_with_label("×");
	gtk_widget_set_tooltip_text(close, "Close");
	gtk_widget_set_valign(close, GTK_ALIGN_START);
	g_signal_connect(close, "clicked", G_CALLBACK(dismiss), NULL);
	gtk_box_pack_end(GTK_BOX(row), close, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(content), row, FALSE, FALSE, 0);
	if (*active->body) gtk_box_pack_start(GTK_BOX(content), label(active->body, 8), FALSE, FALSE, 0);
	for (guint i = 0; active->actions && active->actions[i] && active->actions[i+1]; i += 2) {
		if (!strcmp(active->actions[i], "default")) continue;
		GtkWidget *button = gtk_button_new();
		gtk_container_add(GTK_CONTAINER(button), label(active->actions[i+1], 2));
		g_signal_connect(button, "clicked", G_CALLBACK(invoke), active->actions[i]);
		gtk_box_pack_start(GTK_BOX(content), button, FALSE, FALSE, 0);
	}
	gtk_widget_show_all(balloon);
	/* Explicit zero and critical notifications persist until dismissed. */
	int timeout = active->timeout < 0 ? (active->urgency == 2 ? 0 : 10000) : active->timeout;
	if (timeout > 0) timer = g_timeout_add(timeout, expire, NULL);
}

static void play_notification_sound(Notification *n)
{ if(!n->silent && n->urgency>0 && !n->sounded) {
  static gint64 last_sound;gint64 now=g_get_monotonic_time();
  if(n->power_event || n->battery || n->urgency==2 || now-last_sound>2*G_USEC_PER_SEC) {
   xp_sound_play(n->power_event>0?XPWM_SOUND_DEVICE_CONNECT:n->power_event<0?XPWM_SOUND_DEVICE_DISCONNECT:
    n->urgency==2?XPWM_SOUND_CRITICAL:n->battery?XPWM_SOUND_WARNING:XPWM_SOUND_NOTIFY);last_sound=now;
  }
 }
 n->sounded=TRUE;
}

static void show_next(void)
{
	if (!connection || active || g_queue_is_empty(&pending)) return;
	active = g_queue_pop_head(&pending);
 play_notification_sound(active);
	render();
}

static void method_call(GDBusConnection *bus, const char *sender, const char *path,
		const char *interface, const char *method, GVariant *parameters,
		GDBusMethodInvocation *invocation, gpointer data)
{
	if (!strcmp(method, "GetServerInformation")) {
		g_dbus_method_invocation_return_value(invocation, g_variant_new("(ssss)", "XPWm Balloons", "XPWm", "1.0", "1.2"));
	} else if (!strcmp(method, "GetCapabilities")) {
		const char *caps[] = {"body", "actions", "sound", NULL};
		g_dbus_method_invocation_return_value(invocation, g_variant_new("(^as)", caps));
	} else if (!strcmp(method, "CloseNotification")) {
		guint id; g_variant_get(parameters, "(u)", &id);
		Notification *n = find_notification(id);
		if (!n) { g_dbus_method_invocation_return_dbus_error(invocation, "org.freedesktop.DBus.Error.InvalidArgs", "Unknown notification"); return; }
		close_notification(n, 3);
		g_dbus_method_invocation_return_value(invocation, NULL);
	} else if (!strcmp(method, "Notify")) {
		const char *app, *icon, *summary, *body;
		guint replaces; gint timeout; char **actions; GVariant *hints;
		g_variant_get(parameters, "(&su&s&s&s^as@a{sv}i)", &app, &replaces, &icon, &summary, &body, &actions, &hints, &timeout);
		Notification *n = replaces ? find_notification(replaces) : NULL;
		if ((!n && pending.length >= 32) || g_strv_length(actions) > 16 || g_strv_length(actions) % 2) {
			g_strfreev(actions); g_variant_unref(hints);
			g_dbus_method_invocation_return_dbus_error(invocation, "org.freedesktop.DBus.Error.LimitsExceeded", "Notification queue or action limit exceeded"); return;
		}
		if (!n) {
			n = g_new0(Notification, 1);
			do { next_id++; } while (!next_id || find_notification(next_id));
			n->id = next_id;
			g_queue_push_tail(&pending, n);
		}
		g_free(n->summary); g_free(n->body); g_strfreev(n->actions);
		const char *title = *summary ? summary : app;
		n->summary = g_utf8_substring(title, 0, MIN(256, g_utf8_strlen(title, -1)));
		n->body = g_utf8_substring(body, 0, MIN(4096, g_utf8_strlen(body, -1)));
		n->actions = actions; n->timeout = timeout; n->urgency = 1;
		g_variant_lookup(hints, "urgency", "y", &n->urgency);
  n->silent=FALSE;g_variant_lookup(hints,"suppress-sound","b",&n->silent);
		g_variant_unref(hints);
		g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", n->id));
		if (n == active) render(); else show_next();
	}
}

static const GDBusInterfaceVTable vtable = { .method_call = method_call };

static void acquired(GDBusConnection *bus, const char *name, gpointer data)
{
	connection = g_object_ref(bus);
	registration = g_dbus_connection_register_object(bus, OBJECT, introspection->interfaces[0], &vtable, NULL, NULL, NULL);
 show_next();
}

void xp_notifications_battery_clear(void)
{
 for(GList *l=pending.head,*next;l;l=next) {
  next=l->next;Notification *n=l->data;
  if(n->battery){g_queue_delete_link(&pending,l);free_notification(n);}
 }
 if(active && active->battery)close_notification(active,3);
}

void xp_notifications_battery(int critical,int percent)
{
 xp_notifications_battery_clear();
 Notification *n=g_new0(Notification,1);n->battery=TRUE;
 do {next_id++;}while(!next_id || find_notification(next_id));n->id=next_id;
 n->summary=g_strdup(critical?"Critical Battery":"Low Battery");
 n->body=g_strdup_printf(critical?"Your battery is at %d%%. Connect AC power now and save your work.":"Your battery is at %d%%. Connect AC power soon.",percent);
 n->urgency=critical?2:1;n->timeout=critical?0:15000;
 if(active && (critical || active->urgency<2)) {
  if(timer){g_source_remove(timer);timer=0;}
  gtk_widget_hide(balloon);g_queue_push_head(&pending,active);active=NULL;
 }
 g_queue_push_head(&pending,n);show_next();
}

void xp_notifications_power(int plugged,int percent)
{
 Notification *n=g_new0(Notification,1);
 do {next_id++;}while(!next_id || find_notification(next_id));n->id=next_id;
 n->power_event=plugged?1:-1;
 n->summary=g_strdup(plugged?"AC Power Connected":"Running on Battery");
 n->body=percent>=0?g_strdup_printf(plugged?"Your computer is plugged in. Battery: %d%%.":"AC power was disconnected. Battery: %d%%.",percent):
  g_strdup(plugged?"Your computer is now using AC power.":"Your computer is now using battery power.");
 n->urgency=1;n->timeout=5000;
 g_queue_push_tail(&pending,n);show_next();
}

void xp_notifications_start(void)
{
	/* The session launcher supplies a private bus, including in nested mode. */
	if (g_strcmp0(g_getenv("XPWM_SESSION_BUS"), "1")) return;
	introspection = g_dbus_node_info_new_for_xml(xml, NULL);
	owner = g_bus_own_name(G_BUS_TYPE_SESSION, SERVICE, G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE,
		acquired, NULL, NULL, NULL, NULL);
}

void xp_notifications_stop(void)
{
	if (timer) { g_source_remove(timer); timer = 0; }
	if (balloon) gtk_widget_destroy(balloon);
	balloon = content = NULL;
	if (active) free_notification(active);
	active = NULL;
	g_queue_clear_full(&pending, (GDestroyNotify)free_notification);
	if (owner) g_bus_unown_name(owner);
	if (registration && connection) g_dbus_connection_unregister_object(connection, registration);
	g_clear_object(&connection);
	if (introspection) g_dbus_node_info_unref(introspection);
	introspection = NULL; owner = registration = 0;
}
