#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$project_root"
if [ "${1:-}" = client ]; then
	XPWM_DESKTOP_TEST="$XDG_RUNTIME_DIR/passed" XPWM_ALLOW_NO_WALLPAPER=1 GDK_BACKEND=wayland ./shell/xpwm-desktop
	kill -TERM "$PPID"
	exit 0
fi
test_runtime=$(mktemp -d /tmp/xpwm-desktop-test.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 15 ./compositor/dwl -s 'sh tests/desktop.sh client'
test -f "$test_runtime/passed"
cat "$test_runtime/passed"
