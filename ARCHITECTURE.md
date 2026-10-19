# XPWm architecture

Target: Windows XP SP3 behavior and Luna Blue fidelity on native Wayland.
Priorities: behavior, pixel accuracy, simplicity, performance, portability,
then features. This is an incremental dwl fork, not an XP-inspired theme.

## Compositor: C + wlroots

compositor/ contains dwl v0.7, pinned to wlroots 0.18. dwl.c owns backend and
output setup, input/seat routing, Client and Monitor state, focus, stacking,
window geometry and the wlroots scene graph. client.h bridges xdg-shell and
optional XWayland clients. config.def.h supplies compiled defaults; generated
config.h is a local override. Keep upstream lifecycle and protocol handling.

Milestone 1 selects dwl's floating layout by default and disables focus-follows-
mouse. It does not mark every client permanently floating: switching to tile
can therefore arrange ordinary windows. Upstream dialog rules still apply.
Optional tiling and tags remain development features.

Milestone 2 should extend the existing per-client scene tree and border drawing
in mapnotify/resize, add caption hit testing to pointer handling, and maintain
explicit minimize/maximize/restore state with saved normal geometry. Fullscreen
is separate from maximize. Negotiate server decorations for xdg-decoration
clients; GTK3 clients that insist on CSD need theme/application integration.
Do not promise forced removal of arbitrary application-drawn titlebars.

The compositor owns decorations, placement, modal/focus constraints, stacking,
move/resize, task activation and switching. Draw raster resources through
wlroots buffers, optionally prepared using Cairo. Do not embed GTK in dwl.
The decoration prototype now lives in decoration.c and decoration-actions.h;
it uses Cairo/Pango buffers in the per-client scene tree, saved normal geometry,
and caption/border input handlers. See docs/DECORATIONS.md. Local Luna captions
and buttons are integrated; outer borders and layout metrics remain provisional.

## Shell: GTK3 + Cairo

The first taskbar is implemented in shell/taskbar.c, with startup in
shell/session.sh. It reserves screen space through layer-shell and uses standard
wlr-foreign-toplevel-management for task state/actions. compositor/foreign-toplevel.h
connects that protocol to Client state. The mode switch remains a compositor
shortcut; no private IPC protocol has been introduced. The taskbar prototype was
brought forward at the user's request. Asset extraction/indexing is now implemented
in tools/xp_assets.py and verified by tools/verify_assets.py; real resource
integration uses tools/prepare_luna.py and the small shared/luna.c Cairo renderer.

shell/start-menu.c now supplies a GTK popup launcher with GIO application
discovery/launch and Home/folder actions. It is rebuilt on each opening so
installed applications are refreshed. The taskbar supplies the triggering input
event for the Wayland popup: the green Start button is the only menu opener.
A GTK menu grid supplies two columns with resource-backed Cairo backgrounds and
an All Programs submenu. Exact allocation, icons and pinned/recent policy remain
pending; session actions live in session-dialog.c rather than the compositor.

shell/run-dialog.c owns the native Run window and file chooser. run-command.c
separates argv parsing/process launch for display-independent tests. Only explicit
OK invokes a launch; no implicit shell or elevation is added. run-history.c stores
a bounded MRU shared by both Run entry points, while GIO handles document,
folder and URL associations.
The dialog is an ordinary Wayland toplevel, not a keyboard-exclusive panel.

shell/session-dialog.c owns the Start footer's session actions. Shutdown actions
map explicit buttons to `systemctl suspend/poweroff/reboot`; Log Off has its own
confirmation and launches the sibling `xpwm-logoff` overlay, which then calls
`loginctl terminate-user`. No action is shell-parsed. XPWM_SESSION_DRY_RUN
records an action for tests and development; production actions are only
attempted after a visible button press.

The shared session-screen renderer supplies the XP Welcome/logoff overlays as
independent native layer-shell clients. It draws the XP blue screen and uses the
source-indexed LogonUI account tile when local assets exist. The Welcome screen
does not authenticate a password: PAM/display-manager authentication remains the
security boundary, and XPWM_SKIP_LOGON bypasses the visual screen for unattended
sessions. `shared/sounds.c` resolves optional extracted WAVs and invokes an
external PipeWire/PulseAudio/ALSA player so audio stays outside the compositor.

Start temporarily requests exclusive layer keyboard focus, committing that state
before its popup grab. Dismissal returns the layer to noninteractive. The existing
layer commit handler in dwl releases stale exclusive focus and restores the top
application when a panel relinquishes keyboard interaction. Locked-session focus
is not cleared by this release path. The private shell-input test covers Start
and Run Escape dismissal and application focus restoration.

shell/ contains straightforward native Wayland programs for taskbar, Start
menu, desktop, Run/shutdown/logon/logoff screens and settings. `xpwm-desktop` owns the
background layer, Bliss wallpaper modes and the initial XP icon column; it is
kept separate from the taskbar so either process can be restarted independently.
Use layer-shell for desktop
and panels, with exclusive zones reserving taskbar space. Use supported foreign
toplevel protocols for task enumeration/actions where possible; introduce a
small private protocol only for demonstrated gaps, with explicit permissions.
Never put window state ownership in both shell and compositor.

Use custom drawing when GTK3 spacing, menus or state rendering cannot match XP.
File manager integration and launcher discovery should use desktop entries and
standard Linux interfaces. Tray support needs modern StatusNotifier integration
and an explicit decision about legacy XEmbed. Shell crash must not terminate
the compositor. Session/autostart supervision is a later milestone.

## Assets and fidelity

assets/ stores resource provenance and eventually theme mappings; tools/ owns
extraction and indexing. Raw originals are immutable inputs. Derived resources
reference source file/hash, PE type, ID/name, language and conversion parameters.
Theme mappings assign semantic roles without renaming source resources.

shared/luna.c is linked into both compositor and taskbar. It owns only cached
PNG strips, theme sizing/state drawing, and process-local Fontconfig registration;
window policy and shell interactions stay in their existing owners. No GTK code
is linked into the compositor. `XPWM_THEME_DIR` selects a local prepared theme;
`XPWM_ASSET_ROOT` and executable-relative lookup keep the default tree stable
when a display manager changes the working directory. Missing assets use
recreated drawing; Microsoft resources/fonts are not bundled.

Measure an XP SP3 reference at known DPI, font settings, theme and resolution.
Record dimensions and colors with resource IDs or screenshot provenance.
Do not turn guesses into constants advertised as XP measurements. Establish a
96-DPI reference first; scaling behavior must be tested separately. Font metrics
depend on the actual font and rasterization; substitutions require tracking.

Luna Blue comes first, then Olive, Silver, Classic, optionally Royale. Keep theme
data separate from window-management policy. No KDE, GNOME Shell, Qt or Electron
dependency is planned. XWayland is an optional compatibility path, started lazily.

## Incremental delivery

1. Build and launch dwl with XP-oriented defaults and a smoke check.
2. Compositor titlebar, icon, caption buttons, active/inactive states and actions.
3. PE extraction/indexing and resource identification.
4. Taskbar with activation/minimization, clock, Quick Launch and tray in stages.
5. Start button/menu and launcher.
6. Desktop wallpaper/icons and context menus.
7. GTK3 theme; then remaining dialogs, settings, session integration and fidelity.

Each step must compile and get relevant runtime checks. Preserve upstream
licenses and keep source changes small enough to review directly.
