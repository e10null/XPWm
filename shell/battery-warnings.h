#ifndef XPWM_BATTERY_WARNINGS_H
#define XPWM_BATTERY_WARNINGS_H
/* One warning at each severity per discharge; recharge above 25% rearms. */
static inline int xp_battery_warning(int *level,int percent,int discharging)
{
 if(percent<0 || percent>100)return 0;
 if(!discharging || percent>25){*level=0;return 0;}
 int next=percent<=5?2:percent<=20?1:0;
 if(next>*level){*level=next;return next;}
 return 0;
}
/* Initial/unknown readings are quiet; only actual online-state edges notify. */
static inline int xp_power_transition(int *previous,int online)
{
 if(online<0)return 0;
 int change=*previous<0 || *previous==online?0:online?1:-1;
 *previous=online;return change;
}
#endif
