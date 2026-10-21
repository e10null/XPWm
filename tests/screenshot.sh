#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
if [ "${1:-}" = client ]; then
 GDK_BACKEND=wayland ./shell/xpwm-screenshot --no-clipboard --output "$XDG_RUNTIME_DIR/full.png"
 GDK_BACKEND=wayland ./shell/xpwm-screenshot --no-clipboard
 set -- "$XDG_RUNTIME_DIR/Pictures/Screenshots/"*.png
 test -s "$1"
 GDK_BACKEND=wayland ./shell/xpwm-screenshot --output "$XDG_RUNTIME_DIR/clipboard.png" &
 capture_pid=$!
 GDK_BACKEND=wayland ./shell/screenshot-test "$XDG_RUNTIME_DIR/full.png"
 kill -TERM "$capture_pid" 2>/dev/null || true
 kill -TERM "$PPID"
 exit 0
fi
shot_runtime=$(mktemp -d /tmp/xpwm-screenshot.XXXXXX)
trap 'rm -rf -- "$shot_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$shot_runtime" XDG_CONFIG_HOME="$shot_runtime/config" XDG_DATA_HOME="$shot_runtime/data"
mkdir -p "$XDG_CONFIG_HOME"
printf 'XDG_PICTURES_DIR="%s/Pictures"\n' "$shot_runtime" > "$XDG_CONFIG_HOME/user-dirs.dirs"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=2 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 20 ./compositor/dwl -s 'sh tests/screenshot.sh client'
test -s "$shot_runtime/passed"
cat "$shot_runtime/passed"
