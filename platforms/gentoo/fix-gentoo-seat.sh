#!/bin/sh
# Refresh installed seat rules after installing elogind, then start XPWm.
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)

for tool in udevadm loginctl make; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'Required command is missing: %s\n' "$tool" >&2
        exit 1
    fi
done

if [ "$(id -u)" = 0 ]; then
    printf '%s\n' 'Run this script as your normal desktop user; it elevates only the udev commands.' >&2
    exit 1
fi

if command -v doas >/dev/null 2>&1; then
    elevate=doas
elif command -v sudo >/dev/null 2>&1; then
    elevate=sudo
else
    printf '%s\n' 'Install doas or sudo so this script can refresh udev device registration.' >&2
    exit 1
fi

printf '%s\n' 'Reloading udev rules and refreshing graphics/input devices (authentication may be required).'
"$elevate" udevadm control --reload-rules
"$elevate" udevadm trigger --action=change --subsystem-match=drm --subsystem-match=input
"$elevate" udevadm settle

found_gpu=0
for device in /dev/dri/card*; do
    [ -c "$device" ] || continue
    if udevadm info --query=property --name="$device" | grep -Eq '^TAGS=.*:seat:'; then
        found_gpu=1
    fi
done
if [ "$found_gpu" != 1 ]; then
    printf '%s\n' 'No GPU has the udev seat tag. Check the installed 71-seat.rules and GPU driver.' >&2
    exit 1
fi

loginctl seat-status seat0
printf '%s\n' 'Starting XPWm. Run this from a logged-in local text console.'
cd "$project_root"
exec make tty
