#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_runtime=$(mktemp -d /tmp/xpwm-lock.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime" XDG_CONFIG_HOME="$test_runtime/config" XPWM_PROJECT_ROOT="$PWD"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=2 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
protocol=$(pkg-config --variable=pkgdatadir wayland-protocols)/staging/ext-session-lock/ext-session-lock-v1.xml
wayland-scanner client-header "$protocol" "$test_runtime/session-lock-client.h"
wayland-scanner private-code "$protocol" "$test_runtime/session-lock-protocol.c"
cc -D_DEFAULT_SOURCE -I"$test_runtime" tests/lock-client.c "$test_runtime/session-lock-protocol.c" $(pkg-config --cflags --libs wayland-client) -o "$test_runtime/swaylock"
export PATH="$test_runtime:$PATH"
# Exercise the production launcher with an isolated protocol fixture backend.
mkdir -p "$test_runtime/project/shell" "$test_runtime/project/tools"
cp tools/lock.sh "$test_runtime/project/tools/lock.sh"
ln -s "$test_runtime/swaylock" "$test_runtime/project/shell/xpwm-lock"
ln -s "$PWD/shell/xpwm-screensaver" "$test_runtime/project/shell/xpwm-screensaver"
export XPWM_PROJECT_ROOT="$test_runtime/project"
timeout 15 ./compositor/lock-policy-test
