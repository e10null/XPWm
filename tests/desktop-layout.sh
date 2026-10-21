#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
if [ "${1:-}" = client ]; then
 for mode in write read read; do
  XPWM_LAYOUT_RESTART_TEST="$mode" XPWM_LAYOUT_RESTART_MARKER="$XDG_RUNTIME_DIR/$mode" XPWM_ALLOW_NO_WALLPAPER=1 GDK_BACKEND=wayland ./shell/xpwm-desktop
 done
 kill -TERM "$PPID"
 exit 0
fi
layout_runtime=$(mktemp -d /tmp/xpwm-layout-test.XXXXXX)
trap 'rm -rf -- "$layout_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$layout_runtime" XDG_CONFIG_HOME="$layout_runtime/config" XDG_DATA_HOME="$layout_runtime/data"
mkdir -p "$XDG_CONFIG_HOME" "$layout_runtime/Desktop"
printf 'XDG_DESKTOP_DIR="%s/Desktop"\n' "$layout_runtime" > "$XDG_CONFIG_HOME/user-dirs.dirs"
printf 'sample\n' > "$layout_runtime/Desktop/Position.txt"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 15 ./compositor/dwl -s 'sh tests/desktop-layout.sh client'
test -f "$layout_runtime/read"
cat "$layout_runtime/read"
