#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
if [ "${1:-}" = client ]; then
 GDK_BACKEND=wayland XPWM_TASKBAR_SETTINGS_TEST=1 XPWM_SOUND_MUTE=1 ./shell/xpwm-taskbar
 kill -TERM "$PPID"
 exit 0
fi
test_runtime=$(mktemp -d /tmp/xpwm-taskbar-settings.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime" XDG_CONFIG_HOME="$test_runtime/config" XDG_DATA_HOME="$test_runtime/data"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 15 ./compositor/taskbar-settings-test
test -s "$test_runtime/settings-passed"
