# Source inside XPWm, or from the project directory for an individual app.
# Every setting is inherited by child processes; host desktop settings are untouched.
xpwm_theme_root=${XPWM_PROJECT_ROOT:-$PWD}
xpwm_assets_root=${XPWM_ASSET_ROOT:-$xpwm_theme_root}
if [ ! -f "$xpwm_theme_root/themes/XPWm-Luna/index.theme" ]; then
    printf '%s\n' 'XPWm theme: set XPWM_PROJECT_ROOT to the project directory.' >&2
    return 1
fi
export GTK_THEME=XPWm-Luna
export QT_FONT_DPI="$(python3 "$xpwm_theme_root/tools/session_fonts.py" --dpi)"
# Ask cooperating GTK/Mozilla clients to use the compositor's Luna frame.
export GTK_CSD=0
export MOZ_GTK_TITLEBAR_DECORATION=system
export XDG_CURRENT_DESKTOP=XPWm
export XDG_SESSION_DESKTOP=xpwm
export XDG_SESSION_TYPE=wayland
export XDG_DATA_DIRS="$xpwm_assets_root/assets/local/share:$xpwm_theme_root:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
if [ -f "$xpwm_assets_root/assets/local/share/themes/XPWm-Luna/gtk-2.0/gtkrc" ]; then
    export GTK2_RC_FILES="$xpwm_assets_root/assets/local/share/themes/XPWm-Luna/gtk-2.0/gtkrc"
else
    export GTK2_RC_FILES="$xpwm_theme_root/themes/XPWm-Luna/gtk-2.0/gtkrc"
fi
if [ -f "$xpwm_assets_root/assets/local/share/fontconfig/fonts.conf" ]; then
    export FONTCONFIG_FILE="$xpwm_assets_root/assets/local/share/fontconfig/fonts.conf"
fi
# Rebuild with the current asset location: imported configs may reference an
# old checkout. Generic fonts apply to cooperating GTK, Qt and terminal clients.
if [ -d "${XDG_RUNTIME_DIR:-}" ] && [ -d "$xpwm_assets_root/assets/local/luna-blue-complete/fonts" ]; then
    xpwm_font_config="$XDG_RUNTIME_DIR/xpwm/fontconfig/fonts.conf"
    if python3 "$xpwm_theme_root/tools/session_fonts.py" "$xpwm_assets_root/assets/local/luna-blue-complete/fonts" "$xpwm_font_config"; then
        export FONTCONFIG_FILE="$xpwm_font_config"
    fi
    unset xpwm_font_config
fi
export XPWM_GTK_CSS="$xpwm_theme_root/themes/XPWm-Luna/gtk-3.0/gtk.css"
if [ -f "$xpwm_assets_root/assets/local/share/themes/XPWm-Luna/gtk-3.0/gtk.css" ]; then
    export XPWM_GTK_CSS="$xpwm_assets_root/assets/local/share/themes/XPWm-Luna/gtk-3.0/gtk.css"
fi
if [ -f "$xpwm_theme_root/shell/xpwm-luna-module.so" ]; then
    case ":${GTK3_MODULES:-}:" in *":$xpwm_theme_root/shell/xpwm-luna-module.so:"*) ;; *)
    export GTK3_MODULES="$xpwm_theme_root/shell/xpwm-luna-module.so${GTK3_MODULES:+:$GTK3_MODULES}" ;;
    esac
fi
if [ -f "$xpwm_assets_root/assets/local/share/icons/XPWm-Luna/index.theme" ]; then
    export XPWM_GTK_ICON_THEME=XPWm-Luna
fi
if [ -n "${XPWM_ASSET_ROOT:-}" ]; then
    if [ -z "${XPWM_THEME_DIR:-}" ] && [ -f "$xpwm_assets_root/assets/local/luna-blue-complete/theme.ini" ]; then
        export XPWM_THEME_DIR="$xpwm_assets_root/assets/local/luna-blue-complete"
    fi
    if [ -z "${XPWM_WALLPAPER:-}" ] && [ -f "$xpwm_assets_root/assets/local/xp-sp3/sources/I386/BLISS.JP_.expanded/bliss.png" ]; then
        export XPWM_WALLPAPER="$xpwm_assets_root/assets/local/xp-sp3/sources/I386/BLISS.JP_.expanded/bliss.png"
    fi
fi
export GTK_OVERLAY_SCROLLING=0
unset xpwm_theme_root xpwm_assets_root
