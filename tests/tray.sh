#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$project_root"
. tools/session-theme.sh
if [ "${1:-}" = client ]; then
    GDK_BACKEND=wayland ./shell/tray-test
    kill -TERM "$PPID"
    exit 0
fi
test_runtime=$(mktemp -d /tmp/xpwm-tray.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime"
mkdir "$test_runtime/bin" "$test_runtime/battery"
mkfifo "$test_runtime/audio-events"
cp tests/tray-backend.py "$test_runtime/bin/pactl"
chmod +x "$test_runtime/bin/pactl"
mkdir -p "$test_runtime/backlight/mock"
printf 400 > "$test_runtime/backlight/mock/brightness"
printf 1000 > "$test_runtime/backlight/mock/max_brightness"
cp tests/tray-backend.py "$test_runtime/bin/brightnessctl"
cp tests/tray-backend.py "$test_runtime/bin/gdbus"
chmod +x "$test_runtime/bin/brightnessctl"
chmod +x "$test_runtime/bin/gdbus"
cp tests/tray-backend.py "$test_runtime/bin/wpctl"
cp tests/tray-backend.py "$test_runtime/bin/pavucontrol"
cp tests/tray-backend.py "$test_runtime/bin/nmcli"
chmod +x "$test_runtime/bin/wpctl" "$test_runtime/bin/nmcli" "$test_runtime/bin/pavucontrol"
printf 45 > "$test_runtime/volume"
printf 0 > "$test_runtime/mute"
printf enabled > "$test_runtime/wifi"
printf full > "$test_runtime/connectivity"
printf 74 > "$test_runtime/battery/capacity"
printf Discharging > "$test_runtime/battery/status"
export PATH="$test_runtime/bin:$PATH"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 15 ./compositor/dwl -s 'sh tests/tray.sh client'
test -s "$test_runtime/passed"
