# XP session screens

`xpwm-logon` and `xpwm-logoff` are small native Wayland layer-shell programs.
They share one Cairo renderer and sit in the compositor's overlay layer, so the
Welcome/logoff surface covers the desktop and owns keyboard focus without putting
GTK in the compositor.

The Welcome screen now follows the extracted LogonUI UIFILE 1000 layout:
a dark 80-pixel top band, the original blue content background, a gradient footer
with its original divider strips, a left product logo/instruction column and a
right account column separated by the original fading vertical divider. Account
frames, the chess tile, Windows XP logo and power artwork are source-indexed
LogonUI bitmaps. Geometry is measured in 96-DPI logical pixels; large displays
expand the blue margins rather than stretching the account tile or logo.

Selecting the already authenticated Linux account displays the XP “welcome”
transition and continues to the desktop. Escape keeps Welcome open. This remains
a session welcome surface, not authentication: the display manager, PAM or TTY
has already authenticated the Linux account. The default screen does not request
a password. `XPWM_LOGON_PASSWORD_PROMPT=1` exposes the visual password-layout
preview (original arrow button and password placement); entered text is not
verified, persisted or used as a Linux password. Logoff uses the product logo and
status text on the same blue bands, without the old invented progress-box UI.


The lower-left power button opens the same modal shutdown confirmation used by
the taskbar; Cancel/Escape returns keyboard focus to Welcome. Turn Off requests
`systemctl poweroff` only after confirmation. Set `XPWM_SESSION_DRY_RUN=1` and `XPWM_SESSION_MARKER=/path` for tests.

`shell/session.sh` shows the Welcome screen before starting the desktop, taskbar
and an optional demo terminal (`XPWM_DEMO=1`). Set `XPWM_SKIP_LOGON=1` for unattended development or a
display manager that already supplies its own greeter. `XPWM_LOGON_AUTO=1` is
provided for headless tests.

Start -> Log Off now opens a separate `Log Off Windows` confirmation. Confirming
it launches `xpwm-logoff`, which displays the XP-style “Logging off...” overlay,
plays the local logoff sound, then calls `loginctl terminate-user` after a short
transition. `XPWM_LOGOFF_DELAY_MS` changes that transition for tests. If the
helper is not installed beside `xpwm-taskbar`, the session dialog retains a
direct `loginctl` fallback.

## Shutdown and logoff panels

The taskbar's session panels are centered, 313×198 native layer surfaces. Their
original XP background and flag come from MSGINA bitmap resources 20142/20143;
shutdown button normal/pressed/hot frames come from MSGINA 20150. Logoff and
Switch User artwork comes from SHELL32 14356. `prepare_shell_bitmaps.py` now
converts these BI_RLE8 resources along with the existing uncompressed bitmaps.
A separate translucent layer dims the entire output, including the taskbar,
and intercepts outside clicks. The panel owns keyboard focus until dismissed;
Escape and Cancel close it and restore the previous application focus. GTK
handles button focus, keyboard activation and mnemonics. No compositor caption
is drawn around the original panel artwork.

Turn Off, Stand By, Restart and Log Off retain the existing explicit Linux
commands. Switch User is visible but disabled because this shell does not
implement an authenticated user-switching session. Error text stays in the panel
with a tooltip containing the full message. Opening one session panel closes the
other, and repeated requests for the same panel reuse it. Missing private artwork
uses Cairo and source-indexed icon fallbacks.

`tests/shell-ui.sh` checks panel dimensions, overlay focus, shade cleanup,
Cancel/Escape dismissal, focus restoration and a native pointer click on Restart.
The harness forces `XPWM_SESSION_DRY_RUN=1` before starting the shell; no session
action is executed. To capture the panels during that test:

```
XPWM_SHUTDOWN_SCREENSHOT=/tmp/xpwm-shutdown.png \
XPWM_LOGOFF_DIALOG_SCREENSHOT=/tmp/xpwm-logoff-dialog.png sh tests/shell-ui.sh
```

The blue resource surfaces and icon states are original; desktop dimming is a
static translucent overlay, rather than XP's animated fade to grayscale.

## Sound roles

`shared/sounds.c` looks for canonical WAV files in `XPWM_SOUND_DIR`,
`$XPWM_ASSET_ROOT/assets/local/sounds`, the development tree, and the directory
beside the shell binary. It tries `pw-play`, `paplay`, then `aplay`; no audio
library is linked into the compositor. `XPWM_SOUND_PLAYER` selects one explicit
player and `XPWM_SOUND_MUTE=1` disables playback.

Use `tools/prepare_sounds.py` with the user's own XP ISO or extracted Windows
directory. It keeps original WAV/WA_ bytes and hashes under `raw/`, writes
canonical `startup.wav`, `logon.wav`, `logoff.wav`, `shutdown.wav`, `menu.wav`,
`notify.wav` and `error.wav` derivatives when matching XP filenames are found,
and records source paths, original format, hashes and WAV metadata in
`index.json`. These local derivatives remain ignored and are never bundled.

The current event mapping is startup/logon on Welcome entry, menu on Start open,
error on failed shell actions, shutdown on poweroff/reboot, and logoff on the
logoff transition. Exact XP mixer volume, overlapping-event policy and the full
System Events registry mapping remain TODO.

## Expanded XP sound events

The newer private `assets/local/sounds-xp` extraction includes the original
XPMIN/XPRESTOR window sounds. These extra event files remain available for manual playback, but automatic
minimize/restore, menu and navigation sounds are disabled. The default scheme
keeps startup/logon/logoff/shutdown and error sounds, so opening apps and menus
remains quiet. Logoff playback starts during its visible
transition, before the session termination timer fires.

`make` prepares this extended sound directory from the supplied XP ISO when it
is available, without changing the existing `sounds` extraction. The original
ISO and raw resources remain unchanged; all 13 canonical roles retain source
hashes in the private ledger. Priority is XPWM_SOUND_DIR, then sounds-xp, then
the older sounds/xp-sounds locations. Startup still plays when Welcome is skipped.

Playback uses an available pw-play, paplay or aplay. Set XPWM_SOUND_MUTE=1 to
mute XP shell sounds or XPWM_SOUND_PLAYER to select an executable. For manual
inspection, `./shell/xpwm-sound --list` lists events and
`./shell/xpwm-sound minimize` plays one. The native taskbar test uses a fake
player and asserts quiet window transitions and XP error dispatch without opening an audio device.
