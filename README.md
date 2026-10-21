# XPWm

A small C/dwl/wlroots Wayland desktop project targeting Windows XP SP3 behavior
and visual fidelity, starting with Luna Blue. The runnable dwl baseline now has
compositor-drawn titlebars, caption buttons and basic window actions, with
floating layout and click-to-focus. The GTK3/Cairo taskbar runs as a separate
shell process. Locally extracted Luna Blue images now render rounded captions, side/bottom
frames, caption buttons, dialog push buttons and the taskbar, with process-local XP fonts. Recreated artwork remains
the fallback when assets are unavailable. The supplied XP ISO is unchanged.
The separate desktop layer loads the extracted Bliss wallpaper (with a Cairo
PNG path that remains usable in minimal/headless environments) and an XP-style
icon column. The green taskbar Start button opens the two-column Start menu;
there is no hidden global menu shortcut. Visible default browser and mail
applications occupy the Start menu's Internet and E-mail slots. All Programs
sorts visible applications into XP-style category folders, with alphabetical
entries inside each folder. Common file manager, editor, calculator and media
player shortcuts fill the front page when installed.
Application notifications appear as pale-yellow XP tray balloons with a pointer,
close button and optional action buttons. The session launcher gives XPWm its
own D-Bus session, including when nested, so these notifications stay in XPWm.
The service implements the [freedesktop notification protocol](https://specifications.freedesktop.org/notification/latest/protocol.html).
Try `notify-send "Windows XP" "Your notification balloons are ready."` inside
the XPWm session. Balloons queue, support updates and expiry, and stay quiet.
See [Luna rendering](docs/LUNA_RENDERING.md) for preparation and remaining gaps.

`make nested` and `make tty` now begin with the XP Welcome surface
(`shell/xpwm-logon`), rebuilt using the original LogonUI layout and artwork, then start the desktop and taskbar after the current Linux
account is selected. Start -> Turn Off Computer and Log Off use original XP blue panels with icon
buttons, a dimmed desktop and modal input. Log Off then shows a matching
`shell/xpwm-logoff` transition. These screens reproduce the shell flow but do not
replace PAM or the display manager; set `XPWM_SKIP_LOGON=1` for unattended runs.
Optional XP event sounds are extracted from private media with
`tools/prepare_sounds.py` and played through the host's `pw-play`, `paplay` or
`aplay` when available. Default sounds are limited to startup/logon/logoff/shutdown and errors.
Opening menus/apps and moving between window states stays quiet; set `XPWM_SOUND_MUTE=1` to mute.

The session launcher applies **XPWm Luna Blue** before starting the compositor,
so GTK apps opened through both shortcuts and Start/Run inherit it,
including original control states, Tahoma and XP folder/document icons. Open
**Start → All Programs → Settings → Luna Theme Preview** to inspect it. Host theme settings
are unchanged. See [application theme support and limits](docs/APP_THEME.md).

## Install on Linux

Clone the source and run the installer as your normal user:

```sh
git clone https://github.com/e10null/XPWm.git
cd XPWm
./install.sh --deps
```

This builds and installs XPWm under `~/.local` and creates `xpwm`,
`xpwm-theme-preview`, `xpwm-import-assets` and a Wayland session entry. Add
`~/.local/bin` to your `PATH` if necessary. Start it from a logged-in TTY with
`~/.local/bin/xpwm tty`, or use `xpwm nested` inside an existing Wayland session.
It does not replace your current window manager or change your file associations.

`--deps` explicitly requests build/runtime packages through pacman, apt, dnf or
zypper, using sudo when needed. Package availability varies by distro release;
**wlroots 0.18.x is required**, and the installer stops if it is unavailable.
Other distributions can install the build dependencies below manually and run
`./install.sh` without `--deps`. Python 3.11+ is required. Dependency recipes for
other distributions are not a claim that they have been tested on real hardware.
Optional desktop apps and ISO extraction tools are installed separately.
The XP-themed locker additionally requires Meson, Ninja, Wayland cursor headers,
and PAM development headers. It builds a presentation variant of swaylock 1.8.6;
its PAM backend and session-lock protocol remain upstream.

Most display managers only discover system session files. For a system-wide
installation and login-screen session entry:

```sh
./install.sh --system --prefix /usr
```

Builds run as your normal user; sudo is requested for system deployment. The
compositor installs privately as `/usr/lib/xpwm/compositor/dwl`, preserving any
separately installed `dwl` executable. Log out and select **XPWm** in your display
manager. Seat/device access still needs the normal logind or seatd setup for your
system; the installer does not change groups, services or authentication.

The public installation includes fallback artwork. To use locally extracted XP
assets, install the [ISO importer requirements](tools/README.md#one-command-iso-import),
then run:

```sh
xpwm-import-assets /path/to/windows-xp-sp3-x86.iso
```

The installed importer defaults to `$XDG_DATA_HOME/xpwm-private` (normally
`~/.local/share/xpwm-private`). Launchers discover that asset root automatically;
`XPWM_ASSET_ROOT` can select a different one. Keep existing imports in place or
use a new `--output` root; overwriting is refused. Imported assets stay separate
from installed programs and are preserved by upgrades and uninstall.

Useful installer options:

```sh
./install.sh --dry-run --deps       # Inspect the plan without changing anything
./install.sh --prefix /custom/path  # Custom absolute install prefix
./install.sh --no-build             # Install existing binaries
./install.sh --prefix /usr --destdir /tmp/xpwm-package --no-build
./install.sh --uninstall            # Remove a user install
./install.sh --system --prefix /usr --uninstall
```

For updates, pull new source and rerun the installer with the same prefix. It
records installed file hashes in `share/xpwm/install-manifest.json`, refuses
unmanaged or modified files during an update, and preserves modified files during
uninstall. User settings and private assets are never removed. `--destdir` stages
files for packaging while launchers retain the requested prefix.

## Recommended lightweight applications

For a small setup with good Luna compatibility, install individual GTK3 apps.
They share libraries with the shell and inherit its theme inside XPWm. This is
our practical starting set, not a benchmark claiming the lowest RAM usage of
all alternatives; installed size also depends on libraries already present.

| Purpose | Recommended app/package | Why it fits XPWm |
| --- | --- | --- |
| Files and desktop folders | [Thunar](https://archlinux.org/packages/extra/x86_64/thunar/) (`thunar`) | GTK3 file manager; already used by Super+E. |
| Command Prompt | [Xfce Terminal](https://archlinux.org/packages/extra/x86_64/xfce4-terminal/) (`xfce4-terminal`) | Themeable GTK3 terminal; preferred by the existing Command Prompt shortcut. |
| Notepad | [Mousepad](https://archlinux.org/packages/extra/x86_64/mousepad/) (`mousepad`) | Simple GTK3 text editor without an IDE. |
| Folder backends | [GVfs](https://archlinux.org/packages/extra/x86_64/gvfs/) (`gvfs`) | Provides GIO support for Trash, computer locations and remote filesystems. |

On Arch Linux, install the starting set with:

```sh
sudo pacman -S --needed thunar xfce4-terminal mousepad gvfs
```

Add only the extras you use:

| Purpose | Optional package | Compatibility and cost |
| --- | --- | --- |
| Pictures | [Viewnior](https://archlinux.org/packages/extra/x86_64/viewnior/) (`viewnior`) | Small GTK3 viewer with a short direct dependency list. |
| Calculator | [galculator](https://archlinux.org/packages/extra/x86_64/galculator/) (`galculator`) | GTK3 calculator with basic and scientific modes. |
| Windows network shares | [GVfs SMB backend](https://archlinux.org/packages/extra/x86_64/gvfs-smb/) (`gvfs-smb`) | Install when using SMB shares; brings additional Samba libraries. Network discovery also depends on the network and available discovery services. |
| Web | [Firefox](https://archlinux.org/packages/extra/x86_64/firefox/) (`firefox`) | Practical modern website compatibility; optional and considerably larger than the basic utilities. Web content and browser chrome do not become an exact XP clone. |
| Audio/video | [mpv](https://archlinux.org/packages/extra/x86_64/mpv/) (`mpv`) | Minimal player interface, but codecs add dependencies; its own controls do not inherit Luna. |

Install packages individually rather than the full `xfce4` or `xfce4-goodies`
groups. XPWm already supplies the desktop, panel, menus, clock and basic tray
controls. Advanced `pavucontrol` and `nm-connection-editor` tools are optional;
keep the audio/network services described below only if those features are needed.
Foot remains the terminal fallback and is required by integration tests, but its
custom interface does not inherit GTK styling.

### Desktop items and file associations

My Computer, My Network Places and Recycle Bin open `computer:///`,
`network:///` and `trash:///` through GIO. Missing GVfs backends can produce
**Operation not supported** even when a file manager is installed. Install the
relevant backends and restart XPWm so its private D-Bus session can discover them.
This adds Linux file-manager functionality; it does not implement native XP
Explorer inside the shell.

If folders open in an IDE or the wrong app, set these defaults after installing
the recommended apps. Run as your normal user. These commands change your
user-wide associations, including outside XPWm:

```sh
xdg-mime default thunar.desktop inode/directory
xdg-mime default org.xfce.mousepad.desktop text/plain
```

Check the folder association with `xdg-mime query default inode/directory`.
To diagnose a backend inside an XPWm terminal, run `gio info trash:///` or
`gio info computer:///`. Other file types need a suitable installed application
and a matching association. These recommendations are based on the linked Arch
package metadata; application versions and total dependency footprints can change.

## Build and check

Dependencies: C compiler, make, pkg-config, wlroots **0.18.x** development files,
Wayland development files and scanner, wayland-protocols, libinput, xkbcommon,
Cairo, PangoCairo, Fontconfig and libdrm development files. The taskbar additionally needs
GTK3, gtk-layer-shell and wayland-client development files.
The session launcher requires `dbus-run-session`; `notify-send` is optional for
sending a test balloon.
The notification area has volume/mute controls (`wpctl` from WirePlumber),
network status and a Wi-Fi switch (`nmcli` from NetworkManager), a clickable
clock/calendar, and a power meter when a battery is present. Scroll over the
speaker to adjust volume. Single-click the speaker for the compact vertical
volume popup with −/+ buttons and a mute checkbox, double-click for the mixer,
and right-click for its XP-style menu. With `pactl` installed, audio-server
events update the slider and mute state promptly after physical volume-key
presses; periodic polling remains a fallback for unavailable services.
On systems with a backlight, the original XP Display icon
opens screen brightness controls with −/+ buttons and a slider; scrolling over
the Display icon adjusts brightness in 10% steps. Brightness uses `brightnessctl` when
available, or `gdbus` with logind/elogind's backlight API. Controls keep at least
1% brightness and report permission failures without requiring a root desktop.
Click the network icon for its
XP-styled controls. Optional `pavucontrol` and `nm-connection-editor` provide
advanced audio and connection settings. Missing services appear unavailable.
Use the pinned upstream version; newer wlroots APIs are not interchangeable.

```
make
make check
```

The checks also require wayland-info, timeout and foot. The smoke check launches a private
headless compositor using pixman, connects a native Wayland client, checks output,
xdg-shell and layer-shell globals, and shuts down. This is not a visual or input
fidelity test. A second integration check runs a real foot client and exercises
caption hit-testing, active/inactive rendering state, button cancellation,
maximize/restore/fullscreen, minimize/recovery, left-edge resize, titlebar move,
and close/unmap cleanup. Input is injected into compositor handlers; physical
input devices and a real multi-monitor session are not covered.

Optional screenshot from that private test display (requires grim):

```
make check XPWM_TEST_SCREENSHOT=/tmp/xpwm-decorations.png
```

New windows cascade inside the work area; dialogs are centered and fixed-size
windows respect their constraints. Alt+F4 closes the focused application.
Drag the titlebar to move, double-click it to maximize/restore, or drag borders
to resize. Caption buttons minimize, maximize/restore, and close. Click a taskbar
button to activate/restore its window, or minimize it if already active.
Super+Tab also cycles/restores windows on the current workspace.
Start -> Run opens a native program-launch dialog with quoted arguments,
Browse and inline launch errors. It uses the XP instruction text, resource icon and Luna push buttons. Its command
semantics are not yet XP-exact; see [shell notes](shell/README.md).
See [decoration notes](docs/DECORATIONS.md) for provisional metrics
and remaining limitations.
Run keeps its compact XP dialog size and shows a dropdown only while typing,
listing executable commands from PATH and previous successful commands.
Type to filter with prefix or fuzzy matching; use Up/Down to select, Tab to fill
in a command (then add arguments), and Enter or a click to launch. Escape closes Run. Discovery runs in the
background each time Run opens. The original history dropdown remains available.

From an existing Wayland session, launch an isolated nested desktop:

```
make nested
```

It starts the Bliss wallpaper/icon desktop and taskbar. Set `XPWM_DEMO=1` to
include the native foot demonstration window.
If the project is launched outside its source directory, set `XPWM_ASSET_ROOT` to
the repository root (the binaries also discover a sibling `assets/` tree automatically).
See [TESTING.md](TESTING.md) for the manual,
headless, VT, VM, Podman and VNC workflows.

For an isolated real-seat test from a logged-in text TTY, run `make tty`.

From a suitable local VT with seat/device permissions:

```
./compositor/dwl -s foot
```

This does not install anything or replace your current desktop. foot is the
terminal for the VT example. Bindings now follow your Hyprland configuration:
Super+Return opens a Command Prompt-style terminal; Super+E opens thunar; Super+D or Super+R opens the native XP Run dialog; Super+left/right drag moves/resizes windows; Super+F toggles fullscreen;
Super+Shift+Q closes a window; Super+Shift+M exits. Super+Shift+Space toggles
WM Mode/Floating Mode. Tap either Windows/Super key to open or close Start;
using it in a keyboard or mouse shortcut does not open Start afterward.
WM Mode tiles ordinary windows, including windows
previously dragged into floating state. Floating Mode restores saved positions
and sizes. Dialogs, fullscreen and maximized windows are exempt. Modes are per
monitor and saved automatically in `~/.config/xpwm/layouts.ini` (or under
`$XDG_CONFIG_HOME/xpwm`). Each monitor restores its last mode at the next login;
monitors without a saved preference start in Floating Mode.
See [KEYBINDINGS.md](KEYBINDINGS.md) for the complete
mapping and pending Hyprland-specific features. In nested mode the host may
intercept keys. These personal bindings take precedence over XP shortcut fidelity.

Edit compositor/config.h for local bindings; it is copied from config.def.h
on the first build and is not overwritten thereafter. If updating defaults,
merge changes into your existing config.h before rebuilding.

Optional XWayland build (requires xcb, xcb-icccm and runtime Xwayland):

```
make clean
make XWAYLAND=-DXWAYLAND XLIBS='xcb xcb-icccm'
```

Clean when switching build flags. The default build is native Wayland only.
XWayland-enabled builds start the X server lazily for X11 clients.

See [ARCHITECTURE.md](ARCHITECTURE.md),
[XP_COMPATIBILITY.md](XP_COMPATIBILITY.md) and
[upstream provenance](compositor/UPSTREAM.md). compositor/ retains upstream
licenses; shell/, tools/ and assets/ describe the next boundaries.

Asset tooling: [extraction instructions](tools/README.md) and
[initial XP resource findings](docs/XP_RESOURCE_FINDINGS.md).

## Source-only publication

The public source tree intentionally excludes the Windows XP ISO, extracted
Microsoft artwork/fonts and generated binaries. They are ignored by
`.gitignore`. To reproduce the visual shell, provide your own XP SP3 media,
run `python3 tools/import-xp-iso.py /path/to/windows-xp-sp3-x86.iso` to prepare
the complete local asset set. The importer preserves the ISO and refuses existing
asset sets; use `--output /path/to/new-root` for a separate extraction. See
[requirements and usage](tools/README.md#one-command-iso-import). The source, tests and provenance
format are intended to be shareable; the private assets are not.

The notification area renders the original XP `stobject.dll` speaker/power and
`netshell.dll` connection icons at their native 16px size. `make` prepares them
from the supplied ISO and indexed resources; `tools/prepare_tray.py` records
resource IDs and hashes under `assets/local/xp-tray/index.json`. Luna's original
19px/7px tray content margins and Tahoma 8 clock are used by the taskbar.

Right-click the desktop → Properties → Settings to select a monitor and change
its resolution, refresh rate or display scaling. Only advertised display modes
are offered, including fractional refresh rates; 100% preserves the XP desktop's
normal size. Apply tests the configuration, then offers Keep Changes or Revert.
An independent helper restores the previous mode after 15 seconds or if the
properties window closes before confirmation. Confirmed per-display settings
are saved in `$XDG_CONFIG_HOME/xpwm/displays.ini` (normally
`~/.config/xpwm/displays.ini`) and restored for matching connected displays at
session startup. Disabled displays and their positions remain unchanged.

The [XP shell source audit](docs/XP_SHELL_AUDIT.md) records implemented behavior,
public comparison references, and the remaining differences from XP.

Desktop files come from your XDG Desktop folder. Select with Ctrl/Shift or a
selection rectangle; drag to reposition, use F2 to rename, Delete to send to
trash, and Ctrl+C/X/V to copy or move files. The context menu adds New Folder,
sorting, Auto Arrange and Align to Grid. Positions persist between sessions.
Arrange Icons By → Align Vertically / Align Horizontally chooses whether icons
fill columns or rows. Changing this preference arranges the existing icons and
saves the layout. Local `.desktop` shortcuts launch their application directly,
using the entry's name, icon and launch command, rather than document associations.
Incoming file drops copy into Desktop or a folder icon. File operations are
cancellable; completed files and partial copies remain after cancellation.

Start → Search supports filename wildcards, text content, size and modified-date
ranges, subfolder and hidden-file options, plus Save/Open Search (`.xpsearch`).
Content matching handles UTF-8 text up to 16 MiB and reports skipped files.
Opening a saved search restores its options; click Search to run it.

Crowded taskbars now group windows from the same application. Click a grouped
button to choose a window; right-click for Restore Group, Minimize Group or Close
Group. Taskbar Properties → Group similar taskbar buttons controls this behavior
and saves your preference. Groups expand automatically when there is room.

Taskbar Properties → Window behavior → Activation follows mouse (X-Mouse)
enables focus on hover without raising windows. The default is click-to-focus.
Apply updates the behavior during the session; Cancel discards unsaved changes.
The preference is stored in `xpwm/taskbar.ini` under `[Windows]` as
`FocusFollowsMouse=true` within your XDG configuration directory.

Volume and brightness keys do not display an automatic overlay: the standard XP
desktop did not provide the Windows 10-style indicator. XP-era overlays came
from vendor software such as [Lenovo Hotkey Features Integration](https://support.lenovo.com/ao/en/downloads/ds101335-hotkey-features-integration-for-windows-xp-thinkpad)
and [Dell QuickSet](https://www.dell.com/support/home/en-us/drivers/driversdetails?driverid=6ym7c).

Desktop → Properties → Screen Saver offers native recreations of XP’s 3D
FlowerBox, 3D Flying Objects, 3D Pipes, 3D Text, Beziers, Blank, Marquee,
My Pictures Slideshow, Mystify, Starfield, and Windows XP logo saver.
Gentoo ASCII DVD uses Fastfetch’s purple/white logo in XP Courier New. It bounces around
the screen and changes colors at each edge. Choose a
saver and wait time, then Apply; Preview tries it immediately. Text and slideshow
folder settings also persist in `~/.config/xpwm/screensaver.ini`. The default is
(None). These are native Cairo renderers with no Wine dependency. The renderer
runs only while active, respects idle inhibitors, and dismisses on input.
Hidden settings previews stop animating; slideshow images load off the UI thread.
Animation follows the display frame clock at approximately 30 fps. Pipes grow
continuously with cached completed segments; software effects use a bounded pixel
budget on large displays.

Secure locking requires `swaylock` with working system authentication. On Gentoo,
run `sh platforms/gentoo/setup-lock.sh` once to install it through Portage and PAM.
Super+Shift+L (also Ctrl+Alt+L) runs the locker directly, rather than sending a
`loginctl` notification without a listening locker. It uses Wayland session-lock;
a locker crash leaves the compositor locked. Unlock with your Linux account
password. The bundled `xpwm-lock` renderer shares the original LogonUI artwork
and typography with the Welcome screen: account tile, masked password box, green
submit arrow, Caps Lock and authentication feedback. Authentication uses the
system `swaylock` PAM service. The Welcome screen at session startup follows the
Linux login; it does not replace display-manager authentication. Display Properties → Screen Saver → “Lock automatically” enables a separate
lock timeout (default 15 minutes), including with saver (None). The saver can
start earlier using its own wait time. At the lock timeout the secure password
screen takes over; it stops the saver only after the session lock is acquired.
Unlocking resets both timers. Automatic locking respects idle inhibitors.
Preview remains a dismissible visual preview. This setting defaults off and is
saved with the screen saver settings. Suspend/resume locking is not configured by
this setting.

See [local resource measurements](docs/PERFORMANCE.md) for RAM, idle CPU, startup
timings and the reproducible isolated profiling commands.

Start → Log Off and Super+Shift+M show the XP blue “Logging off...” screen
before ending the session. It covers every connected monitor. To preview the
screen without logging off, run `~/.local/lib/xpwm/shell/xpwm-logoff --preview`.

GTK interfaces use the imported XP Tahoma and Arial faces, with 96-DPI font
metrics matching the native captions; monitor scaling remains separate. Terminal monospace
preferences remain unchanged; the Gentoo saver explicitly uses XP Courier New. The font configuration follows the current private asset location and only
affects XPWm’s session. Apps with an explicit custom font retain that preference.

Print Screen captures the full desktop. Super+Shift+S (or Super+Shift+F) opens
an XP-style area selector; drag to select, or press Escape/right-click to cancel.
The native `xpwm-screenshot` tool saves PNGs in your XDG Pictures/Screenshots
folder and copies the image to the clipboard, without grim, slurp or Hyprland
scripts. A clipboard helper retains the compressed image until the clipboard is
replaced. Use `xpwm-screenshot --no-clipboard` to save only, `--area` for selection,
or `--output /path/to/image.png` to choose a filename.

Tray battery images come from XP `BATMETER.DLL`, bitmap 301, at their original
16px size with the original magenta transparency key. Brightness uses the
original `DESK.CPL` Display icon (group 100), rather than a custom sun drawing.
Asset provenance and source hashes are recorded in the private `xp-tray/index.json`.

Right-click an application in All Programs or the Start shortcuts to choose
Pin to Start / Unpin from Start, or Add to Desktop. Up to eight custom pins
persist by desktop-entry ID in `~/.config/xpwm/start-pins.ini`. Reopening Start
shows changes; removed/uninstalled applications are omitted. Desktop shortcuts
copy the application's original `.desktop` entry into the XDG Desktop folder,
preserving its launch metadata and refusing to overwrite an existing file.

Night Light is available under desktop right-click → Properties → Night Light.
Install `wlsunset` separately (Gentoo: `doas emerge --ask gui-apps/wlsunset`).
Enable it and click Apply; the default schedule is 19:00–07:00 local time,
with 4000 K at night and 6500 K by day, fading over 30 minutes. The toggle,
hours, and temperature are saved in `~/.config/xpwm/night-light.ini` and restored
at login. It uses the compositor gamma-control protocol; nested sessions depend
on the host compositor/output supporting gamma changes.
