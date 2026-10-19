#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
. tools/session-theme.sh
if [ "${1:-}" = client ]; then
 for title in Alpha Beta Gamma Delta; do foot --app-id=xpwm-grouped --title="$title" sh -c 'sleep 25' & done
 foot --app-id=xpwm-unrelated --title='Unrelated window' sh -c 'sleep 25' &
 GDK_BACKEND=wayland XPWM_TASKBAR_GROUP_TEST=1 XPWM_SOUND_MUTE=1 ./shell/xpwm-taskbar
 kill -TERM "$PPID"
 exit 0
fi
test_runtime=$(mktemp -d /tmp/xpwm-taskbar-groups.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime" XDG_CONFIG_HOME="$test_runtime/config" XDG_DATA_HOME="$test_runtime/data"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 18 ./compositor/taskbar-groups-test
test -s "$test_runtime/group-passed"
