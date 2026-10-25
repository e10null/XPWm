/* A private mock login1 service: these tests cannot reach host power controls. */
#include "../shell/session-power.h"
static GMainLoop *loop;
static int calls;
static const char xml[]="<node><interface name='org.freedesktop.login1.Manager'>"
 "<method name='Suspend'><arg type='b' direction='in'/></method>"
 "<method name='PowerOff'><arg type='b' direction='in'/></method>"
 "<method name='Reboot'><arg type='b' direction='in'/></method>"
 "</interface></node>";
static void method(GDBusConnection *bus,const char *sender,const char *path,const char *iface,
 const char *name,GVariant *args,GDBusMethodInvocation *invocation,gpointer data)
{
 const char *expected[]={"Suspend","PowerOff","Reboot","PowerOff"};
 gboolean interactive=FALSE;g_variant_get(args,"(b)",&interactive);g_assert_true(interactive);
 g_assert_cmpint(calls,<,4);g_assert_cmpstr(name,==,expected[calls++]);
 if(calls==4)g_dbus_method_invocation_return_dbus_error(invocation,"org.freedesktop.login1.AccessDenied","Denied by test policy");
 else g_dbus_method_invocation_return_value(invocation,g_variant_new("()"));
}
static const GDBusInterfaceVTable table={.method_call=method};
static void worker(GTask *task,gpointer source,gpointer data,GCancellable *cancel)
{
 GError *error=NULL;
 for(int action=XPWM_SESSION_STANDBY;action<=XPWM_SESSION_REBOOT;action++) {
  g_assert_true(xp_session_power(action,&error));g_assert_no_error(error);
 }
 g_assert_false(xp_session_power(XPWM_SESSION_POWEROFF,&error));g_assert_nonnull(error);
 g_assert_nonnull(strstr(error->message,"Denied by test policy"));g_clear_error(&error);
 g_task_return_boolean(task,TRUE);
}
static void done(GObject *source,GAsyncResult *result,gpointer data)
{
 GError *error=NULL;g_assert_true(g_task_propagate_boolean(G_TASK(result),&error));g_assert_no_error(error);
 g_main_loop_quit(loop);
}
static gboolean timed_out(gpointer data){g_error("Power mock timed out");return G_SOURCE_REMOVE;}
int main(void)
{
 GError *error=NULL;GTestDBus *test=g_test_dbus_new(G_TEST_DBUS_NONE);g_test_dbus_up(test);
 g_setenv("DBUS_SYSTEM_BUS_ADDRESS",g_test_dbus_get_bus_address(test),TRUE);
 GDBusConnection *bus=g_dbus_connection_new_for_address_sync(g_test_dbus_get_bus_address(test),
  G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT|G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,NULL,NULL,&error);g_assert_no_error(error);
 GVariant *reply=g_dbus_connection_call_sync(bus,"org.freedesktop.DBus","/org/freedesktop/DBus",
  "org.freedesktop.DBus","RequestName",g_variant_new("(su)","org.freedesktop.login1",0),NULL,0,2000,NULL,&error);
 g_assert_no_error(error);g_assert_nonnull(reply);g_variant_unref(reply);
 GDBusNodeInfo *info=g_dbus_node_info_new_for_xml(xml,&error);g_assert_no_error(error);
 guint object=g_dbus_connection_register_object(bus,"/org/freedesktop/login1",info->interfaces[0],&table,NULL,NULL,&error);g_assert_no_error(error);
 loop=g_main_loop_new(NULL,FALSE);guint timer=g_timeout_add_seconds(10,timed_out,NULL);
 GTask *task=g_task_new(NULL,NULL,done,NULL);g_task_run_in_thread(task,worker);g_object_unref(task);g_main_loop_run(loop);
 g_assert_cmpint(calls,==,4);g_source_remove(timer);
 g_dbus_connection_unregister_object(bus,object);g_dbus_node_info_unref(info);g_object_unref(bus);g_main_loop_unref(loop);
 g_test_dbus_down(test);g_object_unref(test);
 g_print("PASS: suspend/poweroff/reboot use login1 with interactive authorization; denials propagate\n");return 0;
}
