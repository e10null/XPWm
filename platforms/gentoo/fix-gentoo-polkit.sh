#!/bin/sh
# Enable polkit's elogind session tracking on Gentoo/OpenRC.
set -eu

if [ "$(id -u)" != 0 ]; then
    if command -v doas >/dev/null 2>&1; then
        exec doas sh "$0"
    fi
    if command -v sudo >/dev/null 2>&1; then
        exec sudo sh "$0"
    fi
    printf '%s\n' 'Run this script as root, or install doas/sudo.' >&2
    exit 1
fi

command -v emerge >/dev/null
command -v pkaction >/dev/null
command -v pkill >/dev/null
use_path=/etc/portage/package.use
if [ ! -f "$use_path" ]; then
    mkdir -p "$use_path"
    use_path="$use_path/xpwm-polkit"
fi
if ! grep -qxF 'sys-auth/polkit elogind -systemd' "$use_path" 2>/dev/null; then
    printf '\nsys-auth/polkit elogind -systemd\n' >> "$use_path"
fi
printf '%s\n' 'Rebuilding polkit with elogind support. Confirm the emerge plan when prompted.'
emerge --ask --oneshot --newuse sys-auth/polkit
if [ -x /etc/init.d/polkit ]; then
    rc-service polkit restart
else
    # Gentoo can start polkit through D-Bus without an OpenRC service.
    pkill -TERM -x polkitd || [ "$?" = 1 ]
    ready=0
    for attempt in 1 2 3 4 5; do
        if pkaction --action-id org.freedesktop.login1.chvt >/dev/null 2>&1; then
            ready=1
            break
        fi
        sleep 1
    done
    if [ "$ready" != 1 ]; then
        printf '%s\n' 'Polkit did not reactivate through D-Bus. Check the system D-Bus service.' >&2
        exit 1
    fi
fi
printf '%s\n' 'Polkit rebuilt and restarted. Start XPWm and try Ctrl+Alt+F3 again.'
