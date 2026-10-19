# Native XP shell

The current source audit is in [XP shell audit](../docs/XP_SHELL_AUDIT.md).
Resource-backed artwork and tested behavior do not imply full Windows XP compatibility.

xpwm-taskbar is a GTK3 native Wayland process using gtk-layer-shell and Cairo.
make nested / make tty start it via session.sh. Set XPWM_DEMO=1 to include a demo terminal.

The bottom panel reserves 30 logical pixels, displays task buttons and the
locale-independent XP-style short clock (the default en-US format is `h:mm AM`).
Clicking an active task minimizes it; clicking another task activates/restores
it. Dragging off a button before release cancels activation.
Right-click an empty part of the taskbar and choose Properties to configure
saved clock/Quick Launch visibility, on-top behavior and auto-hide. Auto-hide
keeps a 2px activation edge and reveals without resizing maximized windows;
menus and the Properties window hold it open. Apply saves immediately; Cancel
discards unapplied changes. Settings live under `$XDG_CONFIG_HOME/xpwm/taskbar.ini`.
Wheel scrolling reveals overflow tasks. Labels ellipsize and have tooltips.
The Start button opens a native GTK popup with XP shell links and categorized, alphabetized
All Programs submenus from visible desktop entries. GIO handles application
launching and desktop-entry field codes without wrapping Exec in a shell.
GTK supplies arrow-key navigation, Escape and outside-click dismissal.
The launcher uses a 380-pixel two-column layout with the original Luna header,
programs, places and footer strips. At 96 DPI the current measured allocation is
494 pixels high: a 64-pixel user header, 36-pixel shell rows, a 30-pixel
All Programs/Run row and a 40-pixel session footer. The left column shows the
default Internet and E-mail applications, when present in the visible application
list, with XP-style bold category labels and secondary application names. Remaining
slots use common file-manager/editor/calculator/media-player roles; a persistent
recent-application database is still pending. The right column follows the XP shell ordering:
Documents, Recent Documents, Pictures, Music, Computer, Network Places,
Printers and Faxes, Control Panel, Help and Support, Search, and Run.
Search opens a native Search Results window with folder selection, recursive
case-insensitive filename matching, `*`/`?` wildcards, optional hidden files,
Stop, sortable results and default-application opening. Searches run off the UI
thread, avoid directory symlink loops, and cap results at 1,000. Content/date/size
filters and Search Companion animation remain pending.
Each visible link uses a source-indexed shell32 icon at its native 16/32-pixel
size; the header uses the extracted LogonUI account tile when available.
The footer exposes Log Off and Turn Off Computer. Turn Off opens the XP-style
shutdown panel with the original blue background and three icon buttons; Log Off
opens a matching `Log Off Windows` panel and then
the full-screen `xpwm-logoff` transition. Both panels dim the output, capture modal
input and dismiss through Cancel/Escape. Switch User remains disabled until real
user switching is supported. Stand By, Turn Off and Restart use
explicit buttons and spawn `systemctl`; the logoff helper calls `loginctl`
after its overlay is visible. Set XPWM_SESSION_DRY_RUN=1 with
XPWM_SESSION_MARKER=/path to record the requested action without changing session
state (the automated test uses this mode).

`xpwm-logon` is the first process started by `session.sh` unless
`XPWM_SKIP_LOGON=1`. It draws the XP Welcome screen as a native Wayland overlay,
uses the original LogonUI two-column layout, logo, bands and account frame, and
accepts the already authenticated Linux account after the user selects it. A
password-layout preview is optional via XPWM_LOGON_PASSWORD_PROMPT=1; it is not a
PAM implementation. The power button opens a confirmation rather than immediately
requesting shutdown. `XPWM_LOGON_AUTO=1` is intended for unattended tests.
`xpwm-logoff` draws the matching “Logging off...” overlay and delays the session
termination by `XPWM_LOGOFF_DELAY_MS` milliseconds (1200 by default).

With `assets/local/luna-blue-complete` (or the older `luna-blue-start` / `luna-blue`) available, the
panel uses original Luna background, Start background, task-button and tray
strips, plus process-local Tahoma, Trebuchet MS and Franklin Gothic Medium
faces extracted from XP. Startup logs identify the exact selected font files.
Without assets it uses recreated drawing. Metrics remain provisional; see
../docs/LUNA_RENDERING.md. The Start button composites Explorer's extracted
25x20 flag bitmap over the three-state Luna background. Desktop, Start-menu and
task buttons use source-indexed XP artwork where mappings are known; fallback
art remains explicit for unknown application icons.

Optional XP event sounds are extracted into the ignored
`assets/local/sounds/` tree with `tools/prepare_sounds.py`. Startup/logon,
shell errors, shutdown/reboot and logoff are mapped to canonical
WAV roles. Playback tries `pw-play`, `paplay`, then `aplay`; set
`XPWM_SOUND_DIR`, `XPWM_SOUND_PLAYER` or `XPWM_SOUND_MUTE=1` as needed. Raw WAV/WA_
provenance and hashes stay in `sounds/index.json` and are never published.
One panel is placed on the compositor-selected output and shows windows across
all monitors/workspaces. Activation switches to the window's workspace.
Application icons are resolved from desktop entries, with an original XP generic
application icon as fallback. Task buttons have Restore/Minimize/Maximize/Close
menus; the empty taskbar has Show Desktop/Show Open Windows. Per-monitor and
workspace filtering, grouping and full taskbar properties remain pending. The
tray supplies volume/network/battery/clock controls and XP notification balloons. Quick Launch is a compact strip for Show Desktop and
Home; Show Desktop toggles its own minimized-window set, preserving windows that
were already minimized, and Home opens the user's
home URI. Exact XP icon art, ordering and user-configurable entries remain
pending. Exact XP Start-menu fidelity, clock preferences and accessibility are
also pending.

Right-clicking the desktop exposes Refresh, Open Home folder and Properties.
Properties has Desktop and Settings tabs: wallpaper preview/position, monitor
selection, resolution, refresh rate and scaling. Display changes use a separate
15-second rollback guard and saved confirmed settings. Themes/Appearance/Screen
Saver tabs and desktop icon arrangement remain pending.

Manual Start-menu check in a test TTY: click Start, navigate All Programs with
arrows, dismiss with Escape/outside click, and launch a harmless application.
Verify Home folder uses your configured file manager and repeated open/dismiss
cycles restore window input. Dragging off Start before release must not open it.
The taskbar test checks menu structure; tests/shell-ui.c additionally routes
pointer events through the compositor and the native Wayland connection. It checks
Start mapping, outside dismissal, reopening and Run activation. Event timestamps
use the monotonic clock so GTK's menu timing receives realistic input.
The same harness advertises a keyboard and routes Escape through dwl's keyboard
handler, checking Start/Run dismissal and keyboard focus returning to a real foot
client. It also checks Home/End column selection, Right opening All Programs,
Left returning from that submenu, crossing to the places column and Enter
activating Run. A private XDG application directory supplies harmless Alpha/Beta
launchers for application-list Home/Down/Up/Down/Enter testing. The harness asserts
that the submenu stays mapped during traversal and checks which fixture launched;
it cannot accidentally launch applications from the user's installed list.
Real-application startup, physical devices and exact XP behavior still need validation.

Home/End choose the first/last enabled item in the current column (left column
when nothing is selected). Right opens a selected submenu before attempting a
column move; Left closes that submenu. While All Programs is open, Home/End and
Up/Down address its enabled visible entries, and Enter launches the selection.
Up/Down currently stop at the list ends; XP wrapping behavior remains unverified.
Item order follows the visual application-
first layout. Exact XP traversal/wrap rules remain a reference-check TODO.
My Recent Documents lists up to 15 existing local recent files, excluding private
items; it uses the shared GTK recent-document store. Network Places opens the
network URI, and Printers opens installed printer settings or local CUPS.
User-configurable pins, persistent recent applications, and exact icon
mapping and XP's full application policy remain TODO. The Start menu is intentionally opened by the
taskbar Start button; no separate Super-key menu binding is installed. Existing
personal shortcuts have not been rebound.

## Run dialog

Start -> Run opens one native GTK3 dialog; opening it again presents the existing
one. Enter a program and arguments (for example `foot` or `thunar /tmp`). OK/Enter
launches it; Cancel/Escape closes the dialog. Empty input disables OK. Launch
errors remain in the dialog, and Browse chooses a local program with its path
quoted automatically. No compositor code or personal shortcut changes are needed.

run-command.c uses GLib argv parsing and asynchronous process creation, not a
shell. Quotes group arguments; variables, tilde, wildcards, pipes and redirects
are not expanded. Explicitly launching a shell is possible, but is never implied.
Programs inherit the shell process environment and start in the user's home
directory. Relative executable paths therefore resolve from home, not the repo.
This is Linux launching behavior, not XP CreateProcess/cmd.exe compatibility.

The Run icon and dialog buttons use original extracted XP artwork. The editable
Open dropdown loads up to 26 successful commands from
`~/.config/xpwm/run-history.ini` (honoring XDG_CONFIG_HOME), with newest-first
ordering and duplicate removal. Existing documents/folders and URLs open through
the configured Linux associations. Successful document opens enter the shared
recent-document store. Exact XP dialog units and interactive Browse reference
comparison remain pending. Text uses the original XP
instructions; command launching still follows the Linux rules above. The automated suite tests
argv parsing, a controlled child launch, dialog singleton/empty/error states and
cancel/reopen; it does not prove physical keyboard input or pixel fidelity.

The panel takes keyboard focus only while Start is open. Its layer-state request
is committed before creating the popup grab; closing Start releases that request.
The compositor restores application focus when an interactive layer releases it,
without clearing a session lock's focus. Windows reserve the panel's area when tiled or
maximized; fullscreen draws above it. Its process may exit without taking down
the compositor; the startup process group is terminated when dwl exits.
Restart manually with ./shell/xpwm-taskbar from an XPWm terminal if needed.
No automatic crash restart or duplicate-instance detection yet.

See [session screen details](../docs/SESSION_SCREENS.md) for the Welcome/logoff
flow, authentication boundary and sound-role environment variables.

Window enumeration/actions use wlr-foreign-toplevel-management v3. The compositor
rejects actions while locked. As with dwl's existing control protocols, connected
Wayland clients are trusted; this is not a sandbox for hostile applications.

Tests exercise actual protocol enumeration, minimize, activation and close with
a real foot window. The compositor integration harness checks panel reservation,
maximize geometry and mode round-trips. Original upstream protocol XML (including
its license) is vendored from swaywm/wlr-protocols under compositor/protocols/.

Host gtk-3.0/gtk.css is still loaded by GTK. Outdated imported themes can emit
parsing warnings; the panel itself is custom-drawn. No host config is edited.

Application theming is inherited from session.sh: see [Luna app theme](../docs/APP_THEME.md)
for GTK3/GTK2 support, the local GTK3 module, source-only fallbacks and toolkit limits.
