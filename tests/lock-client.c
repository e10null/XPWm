/* Protocol fixture only: never installed as an actual locker. */
#include <wayland-client.h>
#include "session-lock-client.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static struct ext_session_lock_manager_v1 *manager;
static int acquired;
static void locked(void *data,struct ext_session_lock_v1 *lock) {(void)data;(void)lock;acquired=1;}
static void finished(void *data,struct ext_session_lock_v1 *lock) {(void)data;(void)lock;exit(2);}
static const struct ext_session_lock_v1_listener listener={locked,finished};
static void global(void *data,struct wl_registry *registry,uint32_t name,const char *interface,uint32_t version) {
 (void)data;(void)version;if(!strcmp(interface,"ext_session_lock_manager_v1"))manager=wl_registry_bind(registry,name,&ext_session_lock_manager_v1_interface,1);
}
static void removed(void *data,struct wl_registry *registry,uint32_t name){(void)data;(void)registry;(void)name;}
static const struct wl_registry_listener globals={global,removed};
int main(void) {
 struct wl_display *display=wl_display_connect(NULL);assert(display);
 struct wl_registry *registry=wl_display_get_registry(display);wl_registry_add_listener(registry,&globals,NULL);
 assert(wl_display_roundtrip(display)>=0);assert(manager);
 struct ext_session_lock_v1 *lock=ext_session_lock_manager_v1_lock(manager);ext_session_lock_v1_add_listener(lock,&listener,NULL);
 while(!acquired)assert(wl_display_dispatch(display)>=0);
 char path[1024];snprintf(path,sizeof path,"%s/lock-acquired",getenv("XDG_RUNTIME_DIR"));
 FILE *marker=fopen(path,"w");assert(marker);fprintf(marker,"%ld",(long)getpid());fclose(marker);
 snprintf(path,sizeof path,"%s/lock-release",getenv("XDG_RUNTIME_DIR"));
 while(access(path,F_OK)!=0)usleep(10000);
 ext_session_lock_v1_unlock_and_destroy(lock);wl_display_roundtrip(display);wl_display_disconnect(display);return 0;
}
