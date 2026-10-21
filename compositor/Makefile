.POSIX:
.SUFFIXES:

include config.mk

# flags for compiling
DWLCPPFLAGS = -I. -DWLR_USE_UNSTABLE -D_POSIX_C_SOURCE=200809L \
	-DVERSION=\"$(VERSION)\" $(XWAYLAND)
DWLDEVCFLAGS = -g -pedantic -Wall -Wextra -Wdeclaration-after-statement \
	-Wno-unused-parameter -Wshadow -Wunused-macros -Werror=strict-prototypes \
	-Werror=implicit -Werror=return-type -Werror=incompatible-pointer-types \
	-Wfloat-conversion

# CFLAGS / LDFLAGS
PKGS      = wlroots-0.18 wayland-server xkbcommon libinput cairo pangocairo fontconfig libdrm $(XLIBS)
DWLCFLAGS = `$(PKG_CONFIG) --cflags $(PKGS)` $(DWLCPPFLAGS) $(DWLDEVCFLAGS) $(CFLAGS)
LDLIBS    = `$(PKG_CONFIG) --libs $(PKGS)` -lm $(LIBS)

all: dwl
screensaver-test: dwl.o util.o decoration.o luna.o ../tests/screensaver.c
	$(CC) $(DWLCFLAGS) ../tests/screensaver.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@
start-key-test: dwl.o util.o decoration.o luna.o ../tests/start-key.c
	$(CC) $(DWLCFLAGS) ../tests/start-key.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@
layout-persistence-test: dwl.o util.o decoration.o luna.o ../tests/layout-persistence.c
	$(CC) $(DWLCFLAGS) ../tests/layout-persistence.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@
hover-focus-test: dwl.o util.o decoration.o luna.o ../tests/hover-focus.c
	$(CC) $(DWLCFLAGS) ../tests/hover-focus.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@
media-keys-test: dwl.o util.o decoration.o luna.o ../tests/media-keys.c
	$(CC) $(DWLCFLAGS) ../tests/media-keys.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@
vt-keys-test: dwl.o util.o decoration.o luna.o ../tests/vt-keys.c
	$(CC) $(DWLCFLAGS) ../tests/vt-keys.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@
dwl: dwl.o util.o decoration.o luna.o
	$(CC) dwl.o util.o decoration.o luna.o $(DWLCFLAGS) $(LDFLAGS) $(LDLIBS) -o $@
luna.o: ../shared/luna.c ../shared/luna.h
	$(CC) $(DWLCFLAGS) -c ../shared/luna.c -o $@
decoration.o: decoration.c decoration.h ../shared/luna.h
window-state-test: dwl.o util.o decoration.o luna.o ../tests/window-state.c
	$(CC) $(DWLCFLAGS) ../tests/window-state.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@
session-ui-test: dwl.o util.o decoration.o luna.o ../tests/session-ui.c
	$(CC) $(DWLCFLAGS) ../tests/session-ui.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@
display-state-test: dwl.o util.o decoration.o luna.o ../tests/display-state.c
	$(CC) $(DWLCFLAGS) ../tests/display-state.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@
shell-ui-test: dwl.o util.o decoration.o luna.o ../tests/shell-ui.c
	$(CC) $(DWLCFLAGS) ../tests/shell-ui.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@
dwl.o: dwl.c screensaver.h ../shared/screensaver-config.h layout-settings.h window-menu.h switcher.h client.h foreign-toplevel.h decoration.h decoration-actions.h keybind-actions.h config.h config.mk cursor-shape-v1-protocol.h \
	pointer-constraints-unstable-v1-protocol.h wlr-layer-shell-unstable-v1-protocol.h \
	wlr-output-power-management-unstable-v1-protocol.h xdg-shell-protocol.h
util.o: util.c util.h

# wayland-scanner is a tool which generates C headers and rigging for Wayland
# protocols, which are specified in XML. wlroots requires you to rig these up
# to your build system yourself and provide them in the include path.
WAYLAND_SCANNER   = `$(PKG_CONFIG) --variable=wayland_scanner wayland-scanner`
WAYLAND_PROTOCOLS = `$(PKG_CONFIG) --variable=pkgdatadir wayland-protocols`

cursor-shape-v1-protocol.h:
	$(WAYLAND_SCANNER) enum-header \
		$(WAYLAND_PROTOCOLS)/staging/cursor-shape/cursor-shape-v1.xml $@
pointer-constraints-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) enum-header \
		$(WAYLAND_PROTOCOLS)/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml $@
wlr-layer-shell-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) enum-header \
		protocols/wlr-layer-shell-unstable-v1.xml $@
wlr-output-power-management-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		protocols/wlr-output-power-management-unstable-v1.xml $@
xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

config.h:
	cp config.def.h $@
clean:
	rm -f lock-theme-test lock-policy-test popup-placement-test screenshot-policy-test screensaver-test
	rm -f start-key-test
	rm -f layout-persistence-test
	rm -f hover-focus-test
	rm -f media-keys-test
	rm -f vt-keys-test
	rm -f layout-state-test dwl session-profile taskbar-groups-test desktop-input-test switcher-test taskbar-settings-test display-state-test window-state-test shell-ui-test session-ui-test *.o *-protocol.h

dist: clean
	mkdir -p dwl-$(VERSION)/compositor dwl-$(VERSION)/shared
	cp -R LICENSE* Makefile CHANGELOG.md README.md client.h config.def.h \
		config.mk protocols dwl.1 dwl.c util.c util.h screensaver.h layout-settings.h keybind-actions.h window-menu.h switcher.h foreign-toplevel.h decoration.c decoration.h decoration-actions.h dwl.desktop \
		dwl-$(VERSION)/compositor
	cp ../shared/screensaver-config.h ../shared/luna.c ../shared/luna.h dwl-$(VERSION)/shared
	tar -caf dwl-$(VERSION).tar.gz dwl-$(VERSION)
	rm -rf dwl-$(VERSION)

install: dwl
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f dwl $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/dwl
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	cp -f dwl.1 $(DESTDIR)$(MANDIR)/man1
	chmod 644 $(DESTDIR)$(MANDIR)/man1/dwl.1
	mkdir -p $(DESTDIR)$(DATADIR)/wayland-sessions
	cp -f dwl.desktop $(DESTDIR)$(DATADIR)/wayland-sessions/dwl.desktop
	chmod 644 $(DESTDIR)$(DATADIR)/wayland-sessions/dwl.desktop
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/dwl $(DESTDIR)$(MANDIR)/man1/dwl.1 \
		$(DESTDIR)$(DATADIR)/wayland-sessions/dwl.desktop

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CPPFLAGS) $(DWLCFLAGS) -o $@ -c $<

switcher-test: dwl.o util.o decoration.o luna.o ../tests/switcher.c
	$(CC) $(DWLCFLAGS) ../tests/switcher.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@
taskbar-settings-test: dwl.o util.o decoration.o luna.o ../tests/taskbar-settings.c
	$(CC) $(DWLCFLAGS) ../tests/taskbar-settings.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@

desktop-input-test: dwl.o util.o decoration.o luna.o ../tests/desktop-input.c
	$(CC) $(DWLCFLAGS) ../tests/desktop-input.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@

taskbar-groups-test: dwl.o util.o decoration.o luna.o ../tests/taskbar-groups-state.c
	$(CC) $(DWLCFLAGS) ../tests/taskbar-groups-state.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@

session-profile: dwl.o util.o decoration.o luna.o ../tests/session-profile.c
	$(CC) $(DWLCFLAGS) ../tests/session-profile.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@

layout-state-test: dwl.o util.o decoration.o luna.o ../tests/layout-state.c
	$(CC) $(DWLCFLAGS) ../tests/layout-state.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@

screenshot-policy-test: dwl.o util.o decoration.o luna.o ../tests/screenshot-policy.c
	$(CC) $(DWLCFLAGS) ../tests/screenshot-policy.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@

popup-placement-test: dwl.o util.o decoration.o luna.o ../tests/popup-placement.c
	$(CC) $(DWLCFLAGS) ../tests/popup-placement.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@

lock-policy-test: dwl.o util.o decoration.o luna.o ../tests/lock-policy.c
	$(CC) $(DWLCFLAGS) ../tests/lock-policy.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@

lock-theme-test: dwl.o util.o decoration.o luna.o ../tests/lock-theme.c
	$(CC) $(DWLCFLAGS) ../tests/lock-theme.c util.o decoration.o luna.o $(LDFLAGS) $(LDLIBS) -o $@
