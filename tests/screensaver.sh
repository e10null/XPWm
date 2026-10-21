#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_runtime=$(mktemp -d /tmp/xpwm-screensaver.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime" XDG_CONFIG_HOME="$test_runtime/config" XDG_DATA_HOME="$test_runtime/data"
export XPWM_PROJECT_ROOT="$PWD"
# Presence-only fixture for the settings test; never used as a real locker.
mkdir -p "$test_runtime/bin"
ln -s /bin/false "$test_runtime/bin/swaylock"
export PATH="$test_runtime/bin:$PATH"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=2 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 20 ./compositor/screensaver-test
