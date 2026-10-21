#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$project_root"
test_runtime=$(mktemp -d /tmp/xpwm-welcome-ui.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=${XPWM_TEST_OUTPUTS:-1} WLR_RENDERER=pixman
export XPWM_SESSION_MARKER="$test_runtime/session-action"
unset WAYLAND_DISPLAY DISPLAY
timeout 15 ./compositor/session-ui-test
