#ifndef XPWM_SESSION_POWER_H
#define XPWM_SESSION_POWER_H
#include "session-dialog.h"
/* elogind and systemd-logind expose the same login1 interface. */
static inline gboolean xp_session_power(int action,GError **error)
{
 const char *method=action==XPWM_SESSION_STANDBY?"Suspend":
  action==XPWM_SESSION_POWEROFF?"PowerOff":action==XPWM_SESSION_REBOOT?"Reboot":NULL;
 if(!method){g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_ARGUMENT,"Unknown power action");return FALSE;}
 GDBusConnection *bus=g_bus_get_sync(G_BUS_TYPE_SYSTEM,NULL,error);
 if(!bus)return FALSE;
 GVariant *reply=g_dbus_connection_call_sync(bus,"org.freedesktop.login1",
  "/org/freedesktop/login1","org.freedesktop.login1.Manager",method,
  g_variant_new("(b)",TRUE),G_VARIANT_TYPE_UNIT,G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION,
  120000,NULL,error);
 g_object_unref(bus);
 if(!reply)return FALSE;
 g_variant_unref(reply);return TRUE;
}
#endif
