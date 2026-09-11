.PHONY: all clean check check-assets nested nested-empty tty
all:
	$(MAKE) -C compositor
	$(MAKE) -C shell
clean:
	$(MAKE) -C compositor clean
	$(MAKE) -C shell clean
check: all
	$(MAKE) check-assets
	$(MAKE) -C shell run-command-test
	./shell/run-command-test
	sh tests/smoke.sh
	$(MAKE) -C compositor window-state-test
	sh tests/window-state.sh
	sh tests/taskbar.sh
	$(MAKE) -C compositor shell-ui-test
	sh tests/shell-ui.sh
	sh tests/desktop.sh
check-assets:
	python3 -m unittest discover -s tests -p 'test_*.py' -v
	$(MAKE) -C shell luna-render-test
	./shell/luna-render-test
nested: all
	WLR_BACKENDS=wayland ./compositor/dwl -s 'sh shell/session.sh'
nested-empty: all
	WLR_BACKENDS=wayland ./compositor/dwl
tty: all
	env -u WAYLAND_DISPLAY -u DISPLAY -u WLR_BACKENDS ./compositor/dwl -s 'sh shell/session.sh'
