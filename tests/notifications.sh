#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$project_root"
if [ "${1:-}" = client ]; then
    GDK_BACKEND=wayland ./shell/notifications-test --server &
    balloon_pid=$!
    trap 'kill "$balloon_pid" 2>/dev/null || true' EXIT HUP INT TERM
    ./shell/notifications-test
    if command -v notify-send >/dev/null 2>&1; then
        notify-send -t 100 'Windows XP' 'Application notifications use XP balloons.'
    else
        # GLib's real D-Bus client also works on minimal desktops without libnotify tools.
        gdbus call --session --dest org.freedesktop.Notifications \
            --object-path /org/freedesktop/Notifications \
            --method org.freedesktop.Notifications.Notify \
            XPWm 0 '' 'Windows XP' 'Application notifications use XP balloons.' '[]' '{}' 100 >/dev/null
    fi
    kill -TERM "$PPID"
    exit 0
fi
test_runtime=$(mktemp -d /tmp/xpwm-balloons.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 15 dbus-run-session -- ./compositor/dwl -s 'sh tests/notifications.sh client'
