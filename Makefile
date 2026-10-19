.PHONY: all clean check check-assets nested nested-empty tty
all: app-theme xp-sounds xp-tray
	$(MAKE) -C compositor
	$(MAKE) -C shell
clean:
	$(MAKE) -C compositor clean
	$(MAKE) -C shell clean
check: all
	$(MAKE) check-assets
	$(MAKE) -C shell run-command-test
	./shell/run-command-test
	$(MAKE) -C shell run-history-test
	./shell/run-history-test
	$(MAKE) -C shell run-associations-test
	./shell/run-associations-test
	$(MAKE) -C shell start-menu-test
	./shell/start-menu-test
	$(MAKE) -C shell notifications-test
	sh tests/notifications.sh
	$(MAKE) -C shell tray-test
	sh tests/tray.sh
	$(MAKE) -C shell search-test
	sh tests/search.sh
	sh tests/smoke.sh
	$(MAKE) -C compositor window-state-test
	sh tests/window-state.sh
	$(MAKE) -C compositor layout-state-test
	$(MAKE) -C shell layout-client-test
	sh tests/layout-state.sh
	sh tests/taskbar.sh
	$(MAKE) -C compositor switcher-test taskbar-settings-test
	sh tests/switcher.sh
	sh tests/taskbar-settings.sh
	$(MAKE) -C shell taskbar-groups-test
	./shell/taskbar-groups-test
	$(MAKE) -C compositor taskbar-groups-test
	sh tests/taskbar-groups.sh
	sh tests/session-screen.sh
	$(MAKE) -C compositor session-ui-test
	sh tests/session-ui.sh
	sh tests/app-theme.sh
	$(MAKE) -C compositor shell-ui-test
	sh tests/shell-ui.sh
	$(MAKE) -C shell desktop-files-test
	./shell/desktop-files-test
	$(MAKE) -C shell desktop-render-test
	./shell/desktop-render-test
	sh tests/desktop.sh
	$(MAKE) -C compositor desktop-input-test
	sh tests/desktop-input.sh
	$(MAKE) -C shell display-settings-test
	$(MAKE) -C compositor display-state-test
	sh tests/display-state.sh
check-assets:
	python3 -m unittest discover -s tests -p 'test_*.py' -v
	$(MAKE) -C shell luna-render-test
	./shell/luna-render-test
nested: all
	sh tools/start-session.sh nested
nested-empty: all
	WLR_BACKENDS=wayland sh tools/start-session.sh bare
tty: all
	sh tools/start-session.sh tty

.PHONY: app-theme
app-theme:
	@if [ -f assets/local/xp-sp3/theme-references.json ]; then python3 tools/prepare_app_theme.py; fi

.PHONY: xp-sounds
xp-sounds:
	@if [ -f en_windows_xp_professional_with_service_pack_3_x86_cd_x14-80428.iso ] && [ ! -f assets/local/sounds-xp/index.json ]; then python3 tools/prepare_sounds.py en_windows_xp_professional_with_service_pack_3_x86_cd_x14-80428.iso --output assets/local/sounds-xp; fi

.PHONY: xp-tray
xp-tray:
	@if [ -f assets/local/xp-sp3/index.json ] && [ -f en_windows_xp_professional_with_service_pack_3_x86_cd_x14-80428.iso ] && { [ ! -f assets/local/xp-tray/index.json ] || [ tools/prepare_tray.py -nt assets/local/xp-tray/index.json ]; }; then python3 tools/prepare_tray.py; fi
