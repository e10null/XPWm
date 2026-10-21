# Gentoo / OpenRC

Gentoo-specific repair tools and runtime integration live here, separate from
the shared project tools. Run repair scripts from a local console as your normal
user; they request root access through doas (or sudo when available).

- `./platforms/gentoo/fix-gentoo-seat.sh`: reload installed udev rules, refresh
  graphics/input device registration, check GPU seat tags, and run `make tty`.
- `./platforms/gentoo/fix-gentoo-polkit.sh`: enable polkit's `elogind` USE flag,
  rebuild it, and restart the authorization daemon.
- `./platforms/gentoo/setup-gentoo-audio.sh`: configure and install PipeWire,
  WirePlumber, and ALSA utilities in place of the PulseAudio daemon.
- `session.sh`: runtime hook that starts Gentoo's PipeWire launcher when installed
  and audio is not already running. The installer includes this hook; repair
  scripts remain checkout-only.

These scripts do not run automatically during installation. The session hook is
sourced at login and is inactive without Gentoo's PipeWire launcher.
