PKGS = gtk+-3.0 gtk-layer-shell-0 wayland-client fontconfig
CPPFLAGS += $(shell pkg-config --cflags $(PKGS))
CFLAGS += -std=c11 -Wall -Wextra -Wno-unused-parameter -g
LDLIBS += $(shell pkg-config --libs $(PKGS))

all: xpwm-taskbar xpwm-desktop
luna-render-test: ../tests/luna-render.c luna.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
run-command-test: ../tests/run-command.c run-command.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
xpwm-taskbar: taskbar.o start-menu.o run-dialog.o run-command.o session-dialog.o foreign-protocol.o luna.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
xpwm-desktop: desktop.o luna.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
desktop.o: desktop.c
taskbar.o: taskbar.c foreign-client.h ../shared/luna.h start-menu.h run-dialog.h
start-menu.o: start-menu.c start-menu.h run-dialog.h session-dialog.h ../shared/luna.h
run-dialog.o: run-dialog.c run-dialog.h run-command.h
run-command.o: run-command.c run-command.h
session-dialog.o: session-dialog.c session-dialog.h
luna.o: ../shared/luna.c ../shared/luna.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c ../shared/luna.c -o $@
foreign-protocol.o: foreign-protocol.c
foreign-client.h:
	wayland-scanner client-header ../compositor/protocols/wlr-foreign-toplevel-management-unstable-v1.xml $@
foreign-protocol.c:
	wayland-scanner private-code ../compositor/protocols/wlr-foreign-toplevel-management-unstable-v1.xml $@
clean:
	rm -f xpwm-taskbar xpwm-desktop luna-render-test run-command-test *.o foreign-client.h foreign-protocol.c
.PHONY: all clean
