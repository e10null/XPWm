/* Real D-Bus clients and a headless native balloon surface. */
#include "../shared/sounds.h"
static int sound_count;
static enum XpwmSound last_sound;
static void test_sound(enum XpwmSound sound){sound_count++;last_sound=sound;}
#define xp_sound_play test_sound
#include "../shell/notifications.c"
#undef xp_sound_play
static void verify_sound_policy(void)
{
 Notification n={.urgency=1};play_notification_sound(&n);
 g_assert_cmpint(sound_count,==,1);g_assert_cmpint(last_sound,==,XPWM_SOUND_NOTIFY);
 play_notification_sound(&n);g_assert_cmpint(sound_count,==,1);
 n=(Notification){.urgency=2,.silent=TRUE};play_notification_sound(&n);g_assert_cmpint(sound_count,==,1);
 n=(Notification){.urgency=0};play_notification_sound(&n);g_assert_cmpint(sound_count,==,1);
 n=(Notification){.urgency=1};play_notification_sound(&n);g_assert_cmpint(sound_count,==,1);
 n=(Notification){.urgency=2,.battery=TRUE};play_notification_sound(&n);
 g_assert_cmpint(sound_count,==,2);g_assert_cmpint(last_sound,==,XPWM_SOUND_CRITICAL);
 n=(Notification){.urgency=1,.battery=TRUE};play_notification_sound(&n);
 g_assert_cmpint(sound_count,==,3);g_assert_cmpint(last_sound,==,XPWM_SOUND_WARNING);
 play_notification_sound(&n);g_assert_cmpint(sound_count,==,3);
 n=(Notification){.urgency=1,.power_event=1};play_notification_sound(&n);
 g_assert_cmpint(sound_count,==,4);g_assert_cmpint(last_sound,==,XPWM_SOUND_DEVICE_CONNECT);
 n=(Notification){.urgency=1,.power_event=-1};play_notification_sound(&n);
 g_assert_cmpint(sound_count,==,5);g_assert_cmpint(last_sound,==,XPWM_SOUND_DEVICE_DISCONNECT);
 g_print("PASS: notification sounds, silent hints, low urgency, replacement deduplication and burst cooldown\n");
}
#include "../shared/luna.h"

static guint closed_id, closed_reason, action_id;
static char *action_key;

static gboolean inspect(gpointer data)
{
	static gboolean battery_checked;
 if(connection && !battery_checked) {
  battery_checked=TRUE;
  xp_notifications_battery(0,20);g_assert_nonnull(active);g_assert_true(active->battery);
  g_assert_cmpint(active->urgency,==,1);
  xp_notifications_battery(1,5);g_assert_true(active->battery);
  g_assert_cmpint(active->urgency,==,2);g_assert_cmpuint(timer,==,0);
  xp_notifications_battery_clear();g_assert_true(!active || !active->battery);
  for(GList *l=pending.head;l;l=l->next)g_assert_false(((Notification *)l->data)->battery);
  xp_notifications_power(1,80);Notification *power=find_notification(next_id);
  g_assert_nonnull(power);g_assert_cmpstr(power->summary,==,"AC Power Connected");g_assert_cmpint(power->timeout,==,5000);
  close_notification(power,3);
  xp_notifications_power(0,80);power=find_notification(next_id);
  g_assert_nonnull(power);g_assert_cmpstr(power->summary,==,"Running on Battery");close_notification(power,3);
  g_print("PASS: battery balloons, critical escalation, AC clearing and plug/unplug notifications\n");
 }
 if (!active) return G_SOURCE_CONTINUE;
	g_assert_cmpint(gtk_layer_get_keyboard_mode(GTK_WINDOW(balloon)), ==, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
	g_assert_cmpint(gtk_layer_get_exclusive_zone(GTK_WINDOW(balloon)), ==, -1);
	g_assert_cmpint(gtk_widget_get_allocated_width(balloon), >=, 320);
	if (!strcmp(active->summary, "Dismiss")) dismiss(NULL, NULL);
	else if (!strcmp(active->summary, "Action")) invoke(NULL, "open");
	else if (!strcmp(active->summary, "Screenshot") && g_getenv("XPWM_BALLOON_SCREENSHOT")) {
		char *quoted = g_shell_quote(g_getenv("XPWM_BALLOON_SCREENSHOT"));
		char *command = g_strdup_printf("grim %s", quoted);
		g_spawn_command_line_sync(command, NULL, NULL, NULL, NULL);
		g_free(command); g_free(quoted);
		g_unsetenv("XPWM_BALLOON_SCREENSHOT");
	}
	return G_SOURCE_CONTINUE;
}

static void signal_received(GDBusConnection *bus, const char *sender, const char *path,
		const char *interface, const char *signal, GVariant *parameters, gpointer data)
{
	if (!strcmp(signal, "NotificationClosed")) g_variant_get(parameters, "(uu)", &closed_id, &closed_reason);
	if (!strcmp(signal, "ActionInvoked")) {
		g_free(action_key);
		g_variant_get(parameters, "(us)", &action_id, &action_key);
	}
}

static GVariant *call(GDBusConnection *bus, const char *method, GVariant *parameters)
{
	GError *error = NULL;
	GVariant *reply = g_dbus_connection_call_sync(bus, SERVICE, OBJECT, SERVICE,
		method, parameters, NULL, G_DBUS_CALL_FLAGS_NONE, 2000, NULL, &error);
	g_assert_no_error(error); g_assert_nonnull(reply);
	return reply;
}

static guint notify(GDBusConnection *bus, guint replaces, const char *title, int timeout, gboolean actions)
{
	GVariantBuilder hints;
	g_variant_builder_init(&hints, G_VARIANT_TYPE_VARDICT);
	const char *keys[] = {"open", "Open", NULL}, *none[] = {NULL};
	GVariant *reply = call(bus, "Notify", g_variant_new("(susss^asa{sv}i)", "XPWm Test", replaces,
		"dialog-information", title, "This is a Windows XP style notification balloon.\nClick the close button to dismiss it.",
		actions ? keys : none, &hints, timeout));
	guint id; g_variant_get(reply, "(u)", &id); g_variant_unref(reply);
	g_assert_cmpuint(id, >, 0); return id;
}

static void wait_closed(guint id, guint reason)
{
	gint64 deadline = g_get_monotonic_time()+3000000;
	while (closed_id != id && g_get_monotonic_time() < deadline) {
		while (g_main_context_iteration(NULL, FALSE)) {}
		g_usleep(10000);
	}
	g_assert_cmpuint(closed_id, ==, id); g_assert_cmpuint(closed_reason, ==, reason);
}

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "--server")) {
		luna_init(); gtk_init(&argc, &argv);verify_sound_policy();
		g_setenv("XPWM_SESSION_BUS", "1", TRUE);
		xp_notifications_start(); g_timeout_add(80, inspect, NULL);
		gtk_main(); xp_notifications_stop(); return 0;
	}
	GError *error = NULL;
	GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
	g_assert_no_error(error);
	for (int i = 0; i < 100; i++) {
		GVariant *owned = g_dbus_connection_call_sync(bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
			"org.freedesktop.DBus", "NameHasOwner", g_variant_new("(s)", SERVICE), NULL, 0, 1000, NULL, NULL);
		gboolean ready = FALSE;
		if (owned) { g_variant_get(owned, "(b)", &ready); g_variant_unref(owned); }
		if (ready) break;
		g_usleep(20000);
	}
	guint subscription = g_dbus_connection_signal_subscribe(bus, SERVICE, SERVICE, NULL, OBJECT, NULL, 0, signal_received, NULL, NULL);
	GVariant *reply = call(bus, "GetServerInformation", NULL);
	const char *name, *vendor, *version, *spec;
	g_variant_get(reply, "(&s&s&s&s)", &name, &vendor, &version, &spec);
	g_assert_cmpstr(name, ==, "XPWm Balloons"); g_variant_unref(reply);
	reply = call(bus, "GetCapabilities", NULL); g_variant_unref(reply);
	guint first = notify(bus, 0, "Screenshot", 0, FALSE);
	g_usleep(350000);
	g_assert_cmpuint(notify(bus, first, "Updated balloon", 0, FALSE), ==, first);
	guint queued = notify(bus, 0, "Queued", 0, FALSE);
	g_assert_cmpuint(queued, !=, first);
	g_assert_cmpuint(notify(bus, queued, "Updated queue", 0, FALSE), ==, queued);
	reply = call(bus, "CloseNotification", g_variant_new("(u)", first)); g_variant_unref(reply);
	wait_closed(first, 3);
	reply = call(bus, "CloseNotification", g_variant_new("(u)", queued)); g_variant_unref(reply);
	wait_closed(queued, 3);
	guint expired = notify(bus, 0, "Expires", 100, FALSE); wait_closed(expired, 1);
	guint dismissed = notify(bus, 0, "Dismiss", 0, FALSE); wait_closed(dismissed, 2);
	guint action = notify(bus, 0, "Action", 0, TRUE); wait_closed(action, 2);
	g_assert_cmpuint(action_id, ==, action); g_assert_cmpstr(action_key, ==, "open");
	reply = g_dbus_connection_call_sync(bus, SERVICE, OBJECT, SERVICE, "CloseNotification",
		g_variant_new("(u)", action), NULL, 0, 1000, NULL, &error);
	g_assert_null(reply); g_assert_nonnull(error); g_clear_error(&error);
	g_dbus_connection_signal_unsubscribe(bus, subscription);
	g_object_unref(bus); g_free(action_key);
	g_print("PASS: native balloon surface, no keyboard grab, D-Bus delivery, replacement, queue, expiry, dismissal and actions\n");
	return 0;
}
