#!/bin/sh
# Install a user-session PipeWire audio stack on Gentoo/OpenRC.
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
use_path=/etc/portage/package.use
if [ ! -f "$use_path" ]; then
    mkdir -p "$use_path"
    use_path="$use_path/xpwm-audio"
fi
for rule in \
    'media-video/pipewire sound-server pipewire-alsa dbus elogind extra -systemd -system-service' \
    'media-video/wireplumber elogind -systemd -system-service'; do
    if ! grep -qxF "$rule" "$use_path" 2>/dev/null; then
        printf '\n%s\n' "$rule" >> "$use_path"
    fi
done
printf '%s\n' 'Review the package plan: PipeWire replaces pulseaudio-daemon; PulseAudio client libraries remain.'
emerge --ask --newuse media-video/pipewire media-video/wireplumber media-sound/alsa-utils
printf '%s\n' 'Installed. Log out of XPWm and start it again to launch audio as your normal user.'
