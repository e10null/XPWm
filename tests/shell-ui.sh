#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$project_root"
test_runtime=$(mktemp -d /tmp/xpwm-shell-ui.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
export XDG_DATA_HOME="$test_runtime/data" XDG_DATA_DIRS="$test_runtime/empty"
export GSETTINGS_SCHEMA_DIR="$(pkg-config --variable=datadir glib-2.0)/glib-2.0/schemas"
python3 tests/prepare-launchers.py "$XDG_DATA_HOME"
unset WAYLAND_DISPLAY DISPLAY
timeout 30 ./compositor/shell-ui-test
