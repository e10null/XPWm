# Testing XPWm reliably

`make check` now includes a private headless pointer-driven shell test. It opens
Start, dismisses it outside, reopens it, then clicks Run and checks that its native
toplevel appears. Keyboard events also check Escape dismissal in Run and Start,
and application focus restoration to foot. This catches popup lifecycle and
keyboard-focus bugs without borrowing host input. The test additionally opens and
closes All Programs with Right/Left, uses Home/End and column navigation to reach
Run, and activates it with Enter. Application-list traversal and launch use only
private XDG desktop-entry fixtures: Alpha and Beta write a marker, while Hidden
has NoDisplay enabled. The harness keeps the submenu mapped through Home/Down/Up/
Down and verifies Beta launches on Enter. Host applications are excluded from this
test's discovery paths. Physical devices, list overflow and exact XP fidelity
remain outside its coverage.
Capture the actual mapped Start popup (requires grim) with:

```
make check XPWM_MENU_SCREENSHOT=/tmp/xpwm-start.png
```

`make check` also tests XP notification balloons on a private session bus and
headless Wayland output. It verifies delivery, stable IDs when replacing an
active or queued balloon, expiry, dismissal, action signals, and that the
surface never requests keyboard focus or reserves screen space. To inspect it:

```
make -C shell notifications-test
XPWM_BALLOON_SCREENSHOT=/tmp/xpwm-balloon.png sh tests/notifications.sh
```

Tray controls have a separate test using private audio/network command fixtures
and battery readings. It exercises volume, mute, scroll, rapid slider changes,
Wi-Fi controls, calendar/power dialogs and missing services without changing
host volume or network settings:

```
make -C shell tray-test
XPWM_TRAY_SCREENSHOT=/tmp/xpwm-volume.png sh tests/tray.sh
```

Use a nested Wayland session for normal development. It starts XPWm as one
window inside your current Hyprland session, so closing it cannot disrupt the
host desktop and its logs remain in the launching terminal.

```
make nested
```

This now starts the XP Welcome screen, then the XP desktop background, taskbar and
a native `foot` test client automatically. Select the account tile and press
Enter to continue. The default local extraction loads the Bliss wallpaper and draws
the desktop icon column; an empty screen from `make nested-empty` is still expected.
The bundled test scripts normalize their working directory, and the shell also
searches beside its executable. For a relocated asset tree, set
`XPWM_ASSET_ROOT=/path/to/XPWm` (or use `XPWM_THEME_DIR=/path/to/theme`).
Super+Shift+Space switches the current monitor between WM Mode (tiling) and
Floating Mode. Move/resize a window, switch twice, and verify its floating
geometry returns. Taskbar buttons activate/restore windows and minimize the
already-active window. The Start menu opens from the green Start button. The
two Quick Launch buttons after Start are Show Desktop (minimize mapped windows)
and Home (open the user's home URI); they do not alter keybindings.
Right-click the desktop for Refresh, Open Home folder and the XP-styled
Properties panel; changing Stretch/Center/Tile should repaint the wallpaper.
The foot window proves that output, xdg-shell, server decorations, input focus,
and client rendering are working.

Try the titlebar buttons, drag the titlebar and borders, double-click the
titlebar, then minimize and restore with Super+Tab. Use Super+Shift+M to exit
the nested compositor. If Hyprland retains Super, use the pointer actions or
launch the command from a terminal and verify it there. A missing foot client
will be reported in that terminal.

For a repeatable automated check:

```
make check
make check XPWM_TEST_SCREENSHOT=/tmp/xpwm-decorations.png
```

To capture the isolated desktop surface itself, including Bliss and icons:

```
XPWM_DESKTOP_SCREENSHOT=/tmp/xpwm-desktop.png sh tests/desktop.sh
```

The first check uses an isolated headless wlroots output. The second starts a
real native Wayland `foot` client and drives compositor input/state handlers.
It does not need a host display, so it works in CI. The optional screenshot
captures that private output for inspection.
The suite also starts a real taskbar process, checks native protocol window
enumeration/minimize/activation/close, and verifies taskbar work-area reservation
and mode geometry restoration inside the compositor harness.
The session-screen check runs the Welcome and logoff overlays in dry-run mode so
it cannot terminate the test user; set `XPWM_SOUND_MUTE=1` in headless runs when
the host audio server is unavailable.
The decoration screenshot harness starts the desktop background as well, so its
optional capture includes the extracted Bliss wallpaper, desktop icons and the
Luna taskbar.

## Other environments

| Environment | Use it for | Current recommendation |
| --- | --- | --- |
| Nested Wayland | Fast manual behavior and visual checks | Primary workflow |
| Headless wlroots | Automated integration tests and screenshots | `make check` |
| Local VT | DRM/libinput, actual monitors, cursors and session behavior | Later, after nested testing |
| Virtual machine | Clean-distro and real seat testing | Useful before releases |
| Podman | Build/test dependency isolation | Good for CI, poor for interactive compositor testing |
| VNC | Remote viewing | Not a Wayland compositor test path; it changes input/rendering conditions |

For a local VT test, switch to a spare VT and run `./compositor/dwl -s foot`.
Do this only after nested testing, since it takes control of the active seat.
Do not run it from inside your Hyprland terminal.

## Recommended real-session test: a TTY

Use this when testing shortcuts, pointer grabs, fullscreen, focus, window
stacking, multi-monitor behavior, or anything the host compositor may intercept.

1. Save work in the current desktop.
2. Press `Ctrl`+`Alt`+`F3` (or another unused F-key) and log in at the text TTY.
3. Run:

   ```
   cd /path/to/XPWm
   make tty
   ```

`make tty` removes `WAYLAND_DISPLAY`, `DISPLAY`, and an inherited backend
selection, then lets wlroots choose its normal DRM/libinput backend. It starts
the taskbar and foot so the session visibly works immediately. XPWm uses the real seat while
that TTY is active; Hyprland stays running on its own VT and does not receive
your keybindings.

Exit with `Super`+`Shift`+`M`, then return to the graphical session with the
VT key that your display manager or Hyprland session uses, commonly
`Ctrl`+`Alt`+`F1` or `Ctrl`+`Alt`+`F2`. The correct return VT varies by setup.
If `XDG_RUNTIME_DIR` is missing, log out and back into the TTY through the
normal login prompt; PAM/logind normally creates it. Do not invent or point it
at a shared directory.

A VM is the next best manual environment after nested Wayland: use a snapshot,
virtio GPU, a normal Wayland login/VT, and SPICE for display. Keep the initial
VM test native Wayland; enable XWayland separately when validating X11 clients.
Containers do not provide a DRM seat or reliable Wayland input by themselves,
so treat them as build/CI environments rather than the primary desktop test.

Tray integration also checks the compact vertical volume popup, actual tray
clicks, Escape/outside-click dismissal, and right-click menu focus restoration.
The fake-device test verifies that double-click cancels a pending single-click
popup and opens a fake mixer, so it never changes host audio settings.
The volume thumb is decoded from Luna TrackBar.ThumbVert, retaining its native
21x11 frames. Capture the taskbar/popup via `XPWM_TRAY_BAR_SCREENSHOT` and
`XPWM_VOLUME_POPUP_SCREENSHOT` when running `tests/shell-ui.sh`.

`assets/local/xp-tray/volume-popup.json` records the hash and dimensions of
SNDVOL32 DIALOG 106 (50x80 dialog units, 75x130 client pixels at default XP DPI).
Popup layout is based on that resource; it has no application caption or Close
button. The original volume-thumb strip is 21x55 pixels with five 21x11 states.

Display Properties has native output-management coverage using two private
headless outputs advertising 1280x720 at 59.94/60 Hz and 1600x900 at 60 Hz.
The headless backend's fixed-mode fixtures are translated to its custom-timing
API at the backend boundary. Tests cover real commits, fractional scaling,
neighboring-output preservation, Keep/Revert, the independent 15-second timeout,
GUI EOF/parent closure, unsupported mode rejection, persistence and restore.
All configuration files live in the private test directory.

```
make -C shell display-settings-test
make -C compositor display-state-test
sh tests/display-state.sh
XPWM_DISPLAY_SETTINGS_SCREENSHOT=/tmp/xpwm-display-settings.png sh tests/desktop.sh
```

Physical monitor timing support, unplugging a display during confirmation, and
mixed-DPI application rendering still benefit from checks on real hardware.
