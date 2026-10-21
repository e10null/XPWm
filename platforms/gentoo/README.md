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
  and audio is not already running. If the audio server is unreachable but old
  daemon processes survived a crashed login, it restarts them. The installer includes this hook; repair
  scripts remain checkout-only.

These scripts do not run automatically during installation. The session hook is
sourced at login and is inactive without Gentoo's PipeWire launcher.

`setup-keyring-launcher.sh` installs GNOME Keyring without its SSH agent or
systemd integration, enables GCR's GTK/Wayland prompt, and installs `xdg-utils`
without GNOME/Plasma/X helpers. Run it as your normal user; only Portage setup
uses doas. It starts the secrets component and preserves the current browser
choice. It does not edit PAM rules; the keyring can ask for its password once
per login. Then re-run `gh auth login --hostname github.com --web` to store the
token in the keyring. Use a nonempty password when creating the keyring.
