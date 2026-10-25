#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
if [ "${1:-}" = client ]; then
    GDK_BACKEND=wayland ./shell/font-settings-test
    kill -TERM "$PPID"
    exit 0
fi
test_runtime=$(mktemp -d /tmp/xpwm-font-settings.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime" XDG_CONFIG_HOME="$test_runtime/config" XPWM_PROJECT_ROOT="$PWD"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
# Exercise this build rather than also loading an older installed session module.
unset GTK3_MODULES
. tools/session-theme.sh
timeout 15 ./compositor/dwl -s 'sh tests/font-settings.sh client'
