#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$project_root"
if [ "${1:-}" = client ]; then
    foot --app-id=xpwm-taskbar-test --title="Taskbar Alpha" &
    foot --app-id=xpwm-taskbar-test --title="Taskbar Beta" &
    GDK_BACKEND=wayland XPWM_TASKBAR_TEST="$XDG_RUNTIME_DIR/passed" \
        XPWM_SESSION_DRY_RUN=1 XPWM_SESSION_MARKER="$XDG_RUNTIME_DIR/session-action" ./shell/xpwm-taskbar
    kill -TERM "$PPID"
    exit 0
fi
test_runtime=$(mktemp -d /tmp/xpwm-taskbar-test.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime"
export XDG_CONFIG_HOME="$test_runtime/config" XDG_DATA_HOME="$test_runtime/data"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
# Opening and window-state events stay quiet; errors retain their XP sound.
mkdir "$test_runtime/sounds"
touch "$test_runtime/sounds/minimize.wav" "$test_runtime/sounds/restore.wav" "$test_runtime/sounds/error.wav"
cat > "$test_runtime/player" <<'PLAYER'
#!/bin/sh
printf '%s\n' "$1" >> "$XPWM_SOUND_MARKER"
PLAYER
chmod +x "$test_runtime/player"
export XPWM_SOUND_PLAYER="$test_runtime/player" XPWM_SOUND_DIR="$test_runtime/sounds"
export XPWM_SOUND_MARKER="$test_runtime/sound-events"
unset XPWM_SOUND_MUTE
unset WAYLAND_DISPLAY DISPLAY
timeout 15 ./compositor/dwl -s 'sh tests/taskbar.sh client'
test -f "$test_runtime/passed"
cat "$test_runtime/passed"

! grep -Eq '/(minimize|restore|menu|navigation)\.wav$' "$XPWM_SOUND_MARKER"
grep -q '/error.wav$' "$XPWM_SOUND_MARKER"
printf '%s\n' 'PASS: window transitions remain quiet; XP error sound dispatched through fake player'
