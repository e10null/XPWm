#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
popup_runtime=$(mktemp -d /tmp/xpwm-popup.XXXXXX)
trap 'rm -rf -- "$popup_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$popup_runtime" XDG_CONFIG_HOME="$popup_runtime/config" XDG_DATA_HOME="$popup_runtime/data"
export GDK_BACKEND=wayland WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 10 ./compositor/popup-placement-test
test -s "$popup_runtime/popup-passed"
cat "$popup_runtime/popup-passed"
