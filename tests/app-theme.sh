#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$project_root"
if [ "${1:-}" = client ]; then
    # This client deliberately does not source the theme: the compositor must inherit it.
    XPWM_THEME_CSS="$project_root/themes/XPWm-Luna/gtk-3.0/gtk.css"
    if [ -f assets/local/share/themes/XPWm-Luna/gtk-3.0/gtk.css ]; then
        XPWM_THEME_CSS="$project_root/assets/local/share/themes/XPWm-Luna/gtk-3.0/gtk.css"
    fi
    export XPWM_THEME_CSS
    GDK_BACKEND=wayland XPWM_THEME_TEST=1 ./shell/xpwm-theme-preview
    kill -TERM "$PPID"
    exit 0
fi
test_runtime=$(mktemp -d /tmp/xpwm-app-theme.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime"
export XDG_CONFIG_HOME="$test_runtime/config"
export XDG_CACHE_HOME="$test_runtime/cache"
mkdir -p "$XDG_CONFIG_HOME/gtk-3.0"
# A conflicting desktop preference must not change XP session typography.
cat > "$XDG_CONFIG_HOME/gtk-3.0/settings.ini" <<'SETTINGS'
[Settings]
gtk-font-name=Sans 19
gtk-xft-rgba=rgb
gtk-xft-dpi=147456
SETTINGS
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 15 sh tools/start-session.sh bare -s 'sh tests/app-theme.sh client'
