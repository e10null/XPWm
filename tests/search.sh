#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
. tools/session-theme.sh
if [ "${1:-}" = client ]; then
    GDK_BACKEND=wayland ./shell/search-test --gui
    kill -TERM "$PPID"
    exit 0
fi
test_runtime=$(mktemp -d /tmp/xpwm-search.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime" XDG_CONFIG_HOME="$test_runtime/config" XDG_DATA_HOME="$test_runtime/data"
mkdir -p "$test_runtime/theme-data"
ln -s "$PWD/assets/local/share/themes" "$test_runtime/theme-data/themes"
ln -s "$PWD/assets/local/share/icons" "$test_runtime/theme-data/icons"
ln -s "$(pkg-config --variable=datadir glib-2.0)/mime" "$test_runtime/theme-data/mime"
# Newer gdk-pixbuf discovers image decoders through XDG glycin configuration.
if [ -d "$(pkg-config --variable=datadir glib-2.0)/glycin-loaders" ]; then
    ln -s "$(pkg-config --variable=datadir glib-2.0)/glycin-loaders" "$test_runtime/theme-data/glycin-loaders"
fi
export XDG_DATA_DIRS="$test_runtime/theme-data"
export GSETTINGS_SCHEMA_DIR="$(pkg-config --variable=datadir glib-2.0)/glib-2.0/schemas"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 12 ./compositor/dwl -s 'sh tests/search.sh client'
