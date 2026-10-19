#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$project_root"
command -v foot >/dev/null
test_runtime=$(mktemp -d /tmp/xpwm-window-test.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime" XDG_CONFIG_HOME="$test_runtime/config" XDG_DATA_HOME="$test_runtime/data"
mkdir -p "$XDG_CONFIG_HOME" "$test_runtime/Desktop"
printf 'XDG_DESKTOP_DIR="%s/Desktop"\n' "$test_runtime" > "$XDG_CONFIG_HOME/user-dirs.dirs"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 15 ./compositor/window-state-test
