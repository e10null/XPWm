#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
if [ "${1:-}" = client ]; then
 GDK_BACKEND=wayland ./shell/start-actions-test > "$XDG_RUNTIME_DIR/passed"
 kill -TERM "$PPID"
 exit 0
fi
actions_runtime=$(mktemp -d /tmp/xpwm-start-actions.XXXXXX)
trap 'rm -rf -- "$actions_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$actions_runtime" WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 15 ./compositor/dwl -s 'sh tests/start-actions.sh client'
rg '^PASS:' "$actions_runtime/passed"
