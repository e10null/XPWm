#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_runtime=$(mktemp -d /tmp/xpwm-lock-theme.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime" XPWM_PROJECT_ROOT="$PWD"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=2 WLR_RENDERER=pixman
export XPWM_LOCK_SCREENSHOT="$test_runtime/lock.png" XPWM_LOCK_TYPED="$test_runtime/typed.png"
unset WAYLAND_DISPLAY DISPLAY
timeout 15 "${XPWM_LOCK_TEST:-./compositor/lock-theme-test}"
if cmp -s "$XPWM_LOCK_SCREENSHOT" "$XPWM_LOCK_TYPED"; then
    echo 'FAIL: typing did not change the rendered password field' >&2
    exit 1
fi
cp "$XPWM_LOCK_SCREENSHOT" /tmp/xpwm-lock-preview.png
