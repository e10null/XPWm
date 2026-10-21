/* Idle policy only: a native renderer exists solely while the saver is active. */
#include "../shared/screensaver-config.h"
static volatile sig_atomic_t screensaver_pid, screensaver_finished, autolock_pid, autolock_finished;
static gint64 screensaver_last_activity;
static XpSaverConfig screensaver_config;
static struct stat screensaver_stat;
static int screensaver_seen, screensaver_ready;
static uint32_t screensaver_button;
static void screensaver_stop(void)
{
 pid_t pid=(pid_t)screensaver_pid;screensaver_pid=0;
 if(pid>0)kill(pid,SIGTERM);
}
static int screensaver_activity(void)
{
 int active=screensaver_pid>0;screensaver_last_activity=g_get_monotonic_time();
 /* Keep the saver covering the desktop while the secure locker acquires it. */
 if(active && autolock_pid){start_candidate=0;return 1;}
 if(active){screensaver_stop();start_candidate=0;}
 return active;
}
static void screensaver_reaped(pid_t pid)
{if(pid==(pid_t)autolock_pid){autolock_pid=0;autolock_finished=1;}
 if(pid==(pid_t)screensaver_pid){screensaver_pid=0;screensaver_finished=1;}}
static void screensaver_tick(void)
{
 char *path=xp_saver_config_path();struct stat current;int exists=stat(path,&current)==0;
 gint64 now;
 const char *root;
 char *program;
 sigset_t block,old;
 pid_t pid;
 struct wlr_keyboard *keyboard;
 if(!screensaver_ready || exists!=screensaver_seen || (exists &&
    (current.st_ino!=screensaver_stat.st_ino || current.st_mtim.tv_sec!=screensaver_stat.st_mtim.tv_sec || current.st_mtim.tv_nsec!=screensaver_stat.st_mtim.tv_nsec))) {
  screensaver_stop();xp_saver_config_clear(&screensaver_config);xp_saver_config_load(&screensaver_config);
  screensaver_seen=exists;if(exists)screensaver_stat=current;screensaver_ready=1;
  screensaver_last_activity=g_get_monotonic_time();
 }
 g_free(path);
 now=g_get_monotonic_time();
 if(autolock_finished){autolock_finished=0;screensaver_last_activity=now;}
 if(screensaver_finished){screensaver_finished=0;screensaver_last_activity=now;}
 if((locked || idle_notifier->inhibited) && screensaver_pid)screensaver_stop();
 keyboard=wlr_seat_get_keyboard(seat);
 if(locked || idle_notifier->inhibited || seat->drag || seat->pointer_state.button_count || (keyboard && keyboard->num_keycodes)) {screensaver_last_activity=now;return;}
 if(screensaver_config.lock && !autolock_pid && now-screensaver_last_activity>=(gint64)screensaver_config.lock_minutes*60*G_USEC_PER_SEC) {
  root=getenv("XPWM_PROJECT_ROOT"); program=g_build_filename(root?root:".","tools","lock.sh",NULL);
  sigemptyset(&block);sigaddset(&block,SIGCHLD);sigprocmask(SIG_BLOCK,&block,&old);
  pid=fork();
  if(pid==0){sigprocmask(SIG_SETMASK,&old,NULL);setsid();execl("/bin/sh","sh",program,(char *)NULL);_exit(127);}
  if(pid>0)autolock_pid=(sig_atomic_t)pid;
  if(pid<0)screensaver_last_activity=now;
  sigprocmask(SIG_SETMASK,&old,NULL);g_free(program);return;
 }
 if(exclusive_focus || autolock_pid) return;
 if(screensaver_pid || screensaver_config.mode==XP_SAVER_NONE || now-screensaver_last_activity<(gint64)screensaver_config.minutes*60*G_USEC_PER_SEC) return;
 root=getenv("XPWM_PROJECT_ROOT");program=g_build_filename(root?root:".","shell","xpwm-screensaver",NULL);
 if(access(program,X_OK)<0){wlr_log(WLR_ERROR,"Screen saver renderer unavailable: %s",program);screensaver_last_activity=now;g_free(program);return;}
 sigemptyset(&block);sigaddset(&block,SIGCHLD);sigprocmask(SIG_BLOCK,&block,&old);
 pid=fork();
 if(pid==0){sigprocmask(SIG_SETMASK,&old,NULL);setsid();execl(program,program,(char *)NULL);_exit(127);}
 if(pid>0)screensaver_pid=(sig_atomic_t)pid;
 else {wlr_log(WLR_ERROR,"Unable to start screen saver");screensaver_last_activity=now;}
 sigprocmask(SIG_SETMASK,&old,NULL);g_free(program);
}
