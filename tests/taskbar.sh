#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$project_root"
if [ "${1:-}" = client ]; then
    foot --app-id=xpwm-taskbar-test &
    GDK_BACKEND=wayland XPWM_TASKBAR_TEST="$XDG_RUNTIME_DIR/passed" \
        XPWM_SESSION_DRY_RUN=1 XPWM_SESSION_MARKER="$XDG_RUNTIME_DIR/session-action" ./shell/xpwm-taskbar
    kill -TERM "$PPID"
    exit 0
fi
test_runtime=$(mktemp -d /tmp/xpwm-taskbar-test.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 15 ./compositor/dwl -s 'sh tests/taskbar.sh client'
test -f "$test_runtime/passed"
cat "$test_runtime/passed"
