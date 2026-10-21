#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
if [ "${1:-}" = client ]; then
    foot --app-id=xpwm-start-test sh -c 'sleep 30' &
    exec env GDK_BACKEND=wayland XPWM_START_KEY_TEST=1 XPWM_SOUND_MUTE=1 ./shell/xpwm-taskbar
fi
test_runtime=$(mktemp -d /tmp/xpwm-start-key.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime" XDG_CONFIG_HOME="$test_runtime/config" XDG_DATA_HOME="$test_runtime/data"
mkdir -p "$test_runtime/bin" "$XDG_CONFIG_HOME/xpwm"
printf '[Taskbar]\nAutoHide=true\n' > "$XDG_CONFIG_HOME/xpwm/taskbar.ini"
printf '#!/bin/sh\ntouch "$XDG_RUNTIME_DIR/chord-ran"\n' > "$test_runtime/bin/thunar"
chmod +x "$test_runtime/bin/thunar"
export PATH="$test_runtime/bin:$PATH"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 15 ./compositor/start-key-test
