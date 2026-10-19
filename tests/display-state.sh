#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$project_root"
. tools/session-theme.sh
test_runtime=$(mktemp -d /tmp/xpwm-display-test.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime" XDG_CONFIG_HOME="$test_runtime/config"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=2 WLR_RENDERER=pixman
export GDK_BACKEND=wayland
unset WAYLAND_DISPLAY DISPLAY
timeout 45 ./compositor/display-state-test
test -s "$test_runtime/display-passed"
