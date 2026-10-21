#!/bin/sh
set -eu
if [ "${1:-}" = --packages ]; then
    [ "$(id -u)" -eq 0 ] || exit 1
    use_path=/etc/portage/package.use
    if [ ! -f "$use_path" ]; then
        mkdir -p "$use_path"
        use_path="$use_path/xpwm-keyring-launcher"
    fi
    for rule in \
        'gnome-base/gnome-keyring -ssh-agent -systemd' \
        'app-crypt/gcr:0 gtk wayland' \
        'x11-misc/xdg-utils -gnome -plasma -doc -X' \
        'app-text/xmlto text'; do
        if ! grep -qxF "$rule" "$use_path" 2>/dev/null; then
            printf '\n%s\n' "$rule" >> "$use_path"
        fi
    done
    exec emerge --ask --oneshot --newuse gnome-base/gnome-keyring x11-misc/xdg-utils
fi
if [ "$(id -u)" -eq 0 ]; then
    echo 'Run as your normal desktop user; doas is used only for packages.' >&2
    exit 1
fi
doas sh "$0" --packages
for program in gnome-keyring-daemon xdg-open xdg-mime; do
    if ! command -v "$program" >/dev/null 2>&1; then
        printf 'Portage has not finished installing %s. Resolve its reported config changes and rerun this script.\n' "$program" >&2
        exit 1
    fi
done
if [ -n "${DBUS_SESSION_BUS_ADDRESS:-}" ]; then
    dbus-update-activation-environment WAYLAND_DISPLAY XDG_RUNTIME_DIR XDG_CURRENT_DESKTOP
    gnome-keyring-daemon --start --components=secrets >/dev/null
fi
# Retain existing browser preferences; use the installed Firefox only if none exists.
for scheme in http https; do
    if [ -z "$(xdg-mime query default "x-scheme-handler/$scheme")" ] && command -v firefox-bin >/dev/null 2>&1; then
        xdg-mime default firefox-bin.desktop "x-scheme-handler/$scheme"
    fi
done
printf '\nInstalled the secrets-only keyring and xdg-open browser launcher.\n'
printf 'Run: gh auth login --hostname github.com --web\n'
printf 'When prompted, create a keyring with a nonempty password; gh can then save its token securely.\n'
printf 'The keyring may request its password once per login. No PAM login rules were changed.\n'
