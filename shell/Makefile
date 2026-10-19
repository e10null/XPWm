PKGS = gtk+-3.0 gio-unix-2.0 gtk-layer-shell-0 wayland-client fontconfig
CPPFLAGS += $(shell pkg-config --cflags $(PKGS))
CFLAGS += -std=c11 -Wall -Wextra -Wno-unused-parameter -g
LDLIBS += $(shell pkg-config --libs $(PKGS)) -lm

all: xpwm-displayctl xpwm-taskbar xpwm-desktop xpwm-logon xpwm-logoff xpwm-theme-preview xpwm-luna-module.so xpwm-sound xpwm-run
tray-test: ../tests/tray.c tray.c tray.h xp-style.o luna.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) ../tests/tray.c xp-style.o luna.o $(LDLIBS) -o $@
notifications-test: ../tests/notifications.c notifications.c notifications.h luna.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) ../tests/notifications.c luna.o $(LDLIBS) -o $@
start-menu-test: ../tests/start-menu.c start-menu.c search-dialog.o run-dialog.o run-command.o run-history.o session-dialog.o xp-style.o luna.o sound.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) ../tests/start-menu.c search-dialog.o run-dialog.o run-command.o run-history.o session-dialog.o xp-style.o luna.o sound.o $(LDLIBS) -o $@
luna-render-test: ../tests/luna-render.c luna.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
run-command-test: ../tests/run-command.c run-command.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
xpwm-luna-module.so: luna-module.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $< $(LDFLAGS) $(LDLIBS) -o $@
xpwm-run: run-main.o run-dialog.o run-command.o run-history.o xp-style.o luna.o sound.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
run-main.o: run-main.c run-dialog.h ../shared/luna.h
xpwm-sound: sound-player.o sound.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
sound-player.o: sound-player.c ../shared/sounds.h
xpwm-theme-preview: theme-preview.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
xpwm-taskbar: taskbar-groups.o taskbar-settings.o taskbar.o tray.o notifications.o start-menu.o search-dialog.o run-dialog.o run-command.o run-history.o session-dialog.o xp-style.o foreign-protocol.o luna.o sound.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
xpwm-desktop: desktop-files.o desktop.o display-settings.o xp-style.o luna.o sound.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
xpwm-logon: logon-screen.o session-screen.o session-dialog.o xp-style.o luna.o sound.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
xpwm-logoff: logoff-screen.o session-screen.o session-dialog.o xp-style.o luna.o sound.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
xp-style.o: xp-style.c xp-style.h ../shared/luna.h
desktop-files.o: desktop-files.c desktop-files.h
desktop.o: desktop-input-test.h desktop-surface.h desktop-files.h desktop.c xp-style.h display-settings.h
display-settings.o: display-settings.c display-settings.h xp-style.h
taskbar-settings.o: taskbar-settings.c taskbar-settings.h
taskbar.o: taskbar-settings.h taskbar.c tray.h notifications.h foreign-client.h ../shared/luna.h start-menu.h run-dialog.h session-dialog.h ../shared/sounds.h
notifications.o: notifications.c notifications.h
tray.o: tray.c tray.h xp-style.h
start-menu.o: start-menu.c search-dialog.h xp-style.h start-menu.h run-dialog.h session-dialog.h ../shared/luna.h
run-dialog.o: run-dialog.c run-dialog.h run-command.h run-history.h xp-style.h ../shared/luna.h
run-command.o: run-command.c run-command.h
run-history.o: run-history.c run-history.h
session-dialog.o: session-dialog.c session-dialog.h xp-style.h ../shared/luna.h ../shared/sounds.h
logon-screen.o: logon-screen.c session-screen.h
logoff-screen.o: logoff-screen.c session-screen.h
session-screen.o: session-screen.c session-screen.h ../shared/luna.h ../shared/sounds.h
luna.o: ../shared/luna.c ../shared/luna.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c ../shared/luna.c -o $@
sound.o: ../shared/sounds.c ../shared/sounds.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c ../shared/sounds.c -o $@
foreign-protocol.o: foreign-protocol.c
foreign-client.h:
	wayland-scanner client-header ../compositor/protocols/wlr-foreign-toplevel-management-unstable-v1.xml $@
foreign-protocol.c:
	wayland-scanner private-code ../compositor/protocols/wlr-foreign-toplevel-management-unstable-v1.xml $@
clean:
	rm -f layout-client-test desktop-render-test taskbar-groups-test desktop-files-test search-test xpwm-run xpwm-sound xpwm-luna-module.so xpwm-theme-preview xpwm-taskbar xpwm-desktop xpwm-logon xpwm-logoff luna-render-test run-command-test run-history-test run-associations-test start-menu-test notifications-test tray-test *.o foreign-client.h foreign-protocol.c output-client.h output-protocol.c xpwm-displayctl display-settings-test
.PHONY: all clean

xpwm-displayctl: display-control.o output-protocol.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
display-control.o: display-control.c output-client.h
output-client.h: ../compositor/protocols/wlr-output-management-unstable-v1.xml
	wayland-scanner client-header ../compositor/protocols/wlr-output-management-unstable-v1.xml $@
output-protocol.c: ../compositor/protocols/wlr-output-management-unstable-v1.xml
	wayland-scanner private-code ../compositor/protocols/wlr-output-management-unstable-v1.xml $@
output-protocol.o: output-protocol.c

display-settings-test: ../tests/display-settings.c display-settings.c display-settings.h xp-style.o luna.o xpwm-displayctl
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) ../tests/display-settings.c xp-style.o luna.o $(LDLIBS) -o $@

xpwm-desktop: | xpwm-displayctl

run-history-test: ../tests/run-history.c run-history.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

run-associations-test: ../tests/run-associations.c run-command.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

search-dialog.o: search-dialog.c search-dialog.h xp-style.h

search-test: ../tests/search.c search-dialog.c search-dialog.h xp-style.o luna.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) ../tests/search.c xp-style.o luna.o $(LDLIBS) -o $@

desktop-files-test: ../tests/desktop-files.c desktop-files.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

taskbar-groups.o: taskbar-groups.c taskbar-groups.h
taskbar.o: taskbar-groups.h taskbar-group-test.h
taskbar-groups-test: ../tests/taskbar-groups.c taskbar-groups.o taskbar-settings.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

desktop-render-test: ../tests/desktop-render.c desktop.c desktop-surface.h desktop-input-test.h desktop-files.o display-settings.o xp-style.o luna.o sound.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) ../tests/desktop-render.c desktop-files.o display-settings.o xp-style.o luna.o sound.o $(LDLIBS) -o $@

layout-client-test: ../tests/layout-client.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(LDLIBS) -o $@
