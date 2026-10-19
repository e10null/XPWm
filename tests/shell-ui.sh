#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$project_root"
test_runtime=$(mktemp -d /tmp/xpwm-shell-ui.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
export XDG_CONFIG_HOME="$test_runtime/config" XDG_CONFIG_DIRS="$test_runtime/empty-config"
export XPWM_SESSION_MARKER="$test_runtime/session-action"
export XDG_DATA_HOME="$test_runtime/data" XDG_DATA_DIRS="$test_runtime/empty"
# Use the real session's private theme/icons while keeping all desktop entries
# and defaults isolated from the host. GTK's file chooser requires a valid icon theme.
. tools/session-theme.sh
mkdir -p "$test_runtime/theme-data"
ln -s "$project_root/assets/local/share/themes" "$test_runtime/theme-data/themes"
ln -s "$project_root/assets/local/share/icons" "$test_runtime/theme-data/icons"
# Image loaders also need MIME magic; this shares data, not host launchers.
ln -s "$(pkg-config --variable=datadir glib-2.0)/mime" "$test_runtime/theme-data/mime"
# Newer gdk-pixbuf discovers image decoders through XDG glycin configuration.
if [ -d "$(pkg-config --variable=datadir glib-2.0)/glycin-loaders" ]; then
    ln -s "$(pkg-config --variable=datadir glib-2.0)/glycin-loaders" "$test_runtime/theme-data/glycin-loaders"
fi
export XDG_DATA_DIRS="$test_runtime/theme-data:$test_runtime/empty"
export GSETTINGS_SCHEMA_DIR="$(pkg-config --variable=datadir glib-2.0)/glib-2.0/schemas"
mkdir -p "$XDG_CONFIG_HOME" "$test_runtime/Desktop"
printf 'XDG_DESKTOP_DIR="%s/Desktop"\n' "$test_runtime" > "$XDG_CONFIG_HOME/user-dirs.dirs"
python3 tests/prepare-launchers.py "$XDG_DATA_HOME"
export XPWM_MENU_STRUCTURE_TEST=1
unset WAYLAND_DISPLAY DISPLAY
timeout 45 ./compositor/shell-ui-test
