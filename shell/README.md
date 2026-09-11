# Shell prototype

xpwm-taskbar is a GTK3 native Wayland process using gtk-layer-shell and Cairo.
make nested / make tty start it with a demo terminal via session.sh.

The bottom panel reserves 30 logical pixels, displays task buttons and the
locale-independent XP-style short clock (the default en-US format is `h:mm AM`).
Clicking an active task minimizes it; clicking another task activates/restores
it. Dragging off a button before release cancels activation.
Wheel scrolling reveals overflow tasks. Labels ellipsize and have tooltips.
The Start button opens a native GTK popup with XP shell links and an alphabetical
All Programs submenu from visible desktop entries. GIO handles application
launching and desktop-entry field codes without wrapping Exec in a shell.
GTK supplies arrow-key navigation, Escape and outside-click dismissal.
The launcher uses a 380-pixel two-column layout with the original Luna header,
programs, places and footer strips. At 96 DPI the current measured allocation is
494 pixels high: a 64-pixel user header, 36-pixel shell rows, a 30-pixel
All Programs/Run row and a 40-pixel session footer. The left column shows the
first six visible applications alphabetically (the persistent XP pinned/recent
database is still TODO). The right column follows the XP shell ordering:
Documents, Recent Documents, Pictures, Music, Computer, Network Places,
Printers and Faxes, Control Panel, Help and Support, Search, and Run.
Each visible link uses a source-indexed shell32 icon at its native 16/32-pixel
size; the header uses the extracted LogonUI account tile when available.
The footer exposes Log Off and Turn Off Computer; both open the XP-style session
dialog. Stand By, Turn Off, Restart and Log Off use explicit buttons and spawn
`systemctl`/`loginctl` directly. Set XPWM_SESSION_DRY_RUN=1 with
XPWM_SESSION_MARKER=/path to record the requested action without changing session
state (the automated test uses this mode).

With `assets/local/luna-blue-start` (or the older `luna-blue`) available, the
panel uses original Luna background, Start background, task-button and tray
strips, plus process-local Tahoma, Trebuchet MS and Franklin Gothic Medium
faces extracted from XP. Startup logs identify the exact selected font files.
Without assets it uses recreated drawing. Metrics remain provisional; see
../docs/LUNA_RENDERING.md. The Start button composites Explorer's extracted
25x20 flag bitmap over the three-state Luna background. Desktop, Start-menu and
task buttons use source-indexed XP artwork where mappings are known; fallback
art remains explicit for unknown application icons.
One panel is placed on the compositor-selected output and shows windows across
all monitors/workspaces. Activation switches to the window's workspace.
Per-monitor filtering, workspace filtering, real icons, grouping, menus and a
real tray remain pending. Quick Launch is a compact strip for Show Desktop and
Home; Show Desktop minimizes all currently mapped tasks and Home opens the user's
home URI. Exact XP icon art, ordering and user-configurable entries remain
pending. Exact XP Start-menu fidelity, clock preferences and accessibility are
also pending.

Right-clicking the desktop exposes Refresh, Open Home folder and Properties.
Properties is a compact XP-styled panel for Stretch/Center/Tile wallpaper mode;
full Display Properties tabs and desktop icon arrangement are still pending.

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
Pinned/recent applications, categories, exact icon mapping and XP's full
application policy remain TODO. The Start menu is intentionally opened by the
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

History, document/folder/URL association handling, Run icon, exact XP dialog units,
button artwork, focus restoration and interactive Browse verification remain TODO.
Current spacing and compact GTK styling are provisional. The automated suite tests
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

Window enumeration/actions use wlr-foreign-toplevel-management v3. The compositor
rejects actions while locked. As with dwl's existing control protocols, connected
Wayland clients are trusted; this is not a sandbox for hostile applications.

Tests exercise actual protocol enumeration, minimize, activation and close with
a real foot window. The compositor integration harness checks panel reservation,
maximize geometry and mode round-trips. Original upstream protocol XML (including
its license) is vendored from swaywm/wlr-protocols under compositor/protocols/.

Host gtk-3.0/gtk.css is still loaded by GTK. Outdated imported themes can emit
parsing warnings; the panel itself is custom-drawn. No host config is edited.
