#!/bin/sh
# Install with Gentoo's system authentication configuration and package tracking.
set -eu
if [ "$(id -u)" -eq 0 ]; then
    exec emerge --ask --oneshot gui-apps/swaylock
fi
exec doas emerge --ask --oneshot gui-apps/swaylock
