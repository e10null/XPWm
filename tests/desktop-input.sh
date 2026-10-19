#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
. tools/session-theme.sh
if [ "${1:-}" = client ]; then
 foot --title='Desktop focus test' sh -c 'sleep 30' &
 GDK_BACKEND=wayland XPWM_DESKTOP_FILES_TEST=1 XPWM_SOUND_MUTE=1 ./shell/xpwm-desktop
 kill -TERM "$PPID"
 exit 0
fi
test_runtime=$(mktemp -d /tmp/xpwm-desktop-input.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime" XDG_CONFIG_HOME="$test_runtime/config" XDG_DATA_HOME="$test_runtime/data"
mkdir -p "$XDG_CONFIG_HOME" "$test_runtime/Desktop/Folder"
printf 'XDG_DESKTOP_DIR="%s/Desktop"\n' "$test_runtime" > "$XDG_CONFIG_HOME/user-dirs.dirs"
printf 'sample contents\n' > "$test_runtime/Desktop/Sample.txt"
printf 'nested contents\n' > "$test_runtime/Desktop/Folder/Nested.txt"
printf 'hidden contents\n' > "$test_runtime/Desktop/.hidden"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 20 ./compositor/desktop-input-test
test -s "$test_runtime/desktop-input-passed"
