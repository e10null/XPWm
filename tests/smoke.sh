#!/bin/sh
# Exercise a real client connection on a private, software-rendered display.
set -eu
project_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$project_root"
if [ "${1:-}" = client ]; then
    wayland-info > "$XPWM_TEST_DIR/globals"
    grep -q xdg_wm_base "$XPWM_TEST_DIR/globals"
    grep -q wl_output "$XPWM_TEST_DIR/globals"
    grep -q zwlr_layer_shell_v1 "$XPWM_TEST_DIR/globals"
    touch "$XPWM_TEST_DIR/connected"
    kill -TERM "$PPID"
    exit 0
fi
command -v wayland-info >/dev/null
command -v timeout >/dev/null
XPWM_TEST_DIR=$(mktemp -d /tmp/xpwm-smoke.XXXXXX)
export XPWM_TEST_DIR
trap 'rm -rf -- "$XPWM_TEST_DIR"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$XPWM_TEST_DIR"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
if ! timeout 15 ./compositor/dwl -s 'sh tests/smoke.sh client' > "$XPWM_TEST_DIR/status" 2> "$XPWM_TEST_DIR/log"; then
    cat "$XPWM_TEST_DIR/log" >&2
    exit 1
fi
if [ ! -f "$XPWM_TEST_DIR/connected" ]; then
    cat "$XPWM_TEST_DIR/log" >&2
    exit 1
fi
printf '%s\n' 'PASS: headless output, native Wayland connection, xdg-shell, layer-shell, shutdown'
