#ifndef XPWM_NOTIFICATIONS_H
#define XPWM_NOTIFICATIONS_H
void xp_notifications_start(void);
void xp_notifications_battery(int critical,int percent);
void xp_notifications_battery_clear(void);
void xp_notifications_power(int plugged,int percent);
void xp_notifications_stop(void);
#endif
