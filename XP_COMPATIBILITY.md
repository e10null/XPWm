# Windows XP SP3 compatibility tracker

Checked means implemented and verified at the stated scope, not automatically
pixel-identical. Unchecked features are pending, even when dwl has partial support.
Theme-resource measurements are recorded in docs/XP_RESOURCE_FINDINGS.md;
interactive XP reference measurements are still pending. TODO means obtain evidence;
do not substitute modern behavior for an unknown XP behavior.

## Latest source audit

See [XP shell audit](docs/XP_SHELL_AUDIT.md) for public references, concrete code
comparisons, fixes, test coverage and the prioritized remaining gaps. Historical
unchecked full-fidelity items below remain unchecked unless their entire scope
has been established; partial implementation is not pixel equivalence.

- [x] Categorized All Programs and working Recent Documents with private/stale filtering.
- [x] Run MRU persistence and native document/folder/URL association handling.
- [x] Reversible Show Desktop preserving pre-existing minimized windows.
- [x] Task-button Restore/Minimize/Maximize/Close menus and application icon lookup.
- [x] Original tray resources, vertical volume popup and context-menu behavior.
- [x] Saved native taskbar Properties: auto-hide, clock/Quick Launch visibility and on-top behavior.
- [x] Visual Alt+Tab with cancellation, Alt-release activation and safe minimized/closing windows.
- [x] Caption system menu and cancelable keyboard/pointer Move/Size.
- [x] Guarded display resolution, refresh and scaling; confirmed settings persistence.

The follow-up pass fixes anchored refresh dropdowns and original 32-bit speaker
selection, adds XP LAN traffic icon states, and replaces Start's Search placeholder
with native cancellable filename search. See the audit for tested scope and limits.

## Baseline

- [x] dwl/wlroots C source imported with upstream provenance and licenses.
- [x] Native build and headless launch/client connection/shutdown smoke check.
- [x] Default floating layout; optional dwl tiling retained.
- [x] Click-to-focus configured; personal Hyprland shortcuts (see KEYBINDINGS.md).
- [ ] Interactive XP focus/raise/activation behavior verified against reference.
- [ ] Native hardware/VT session and multi-monitor validation.
- [ ] XWayland application behavior validation.

## Window management and decorations

Prototype progress (does not check off the full-fidelity items below):

- [x] Compositor caption with title, generic icon and active/inactive states.
- [x] Caption buttons with hover/press and release-outside cancellation.
- [x] Basic minimize and Super+Tab recovery, maximize/restore, fullscreen transitions.
- [x] Caption move and border resize implementation; runtime test covers left edge.
- [x] Real Wayland client integration test and basic screenshot inspection.
- [x] Local original Luna caption/button strips, state selection and XP font loading.
- [x] Source-indexed rendering derivatives; synthetic transparency/nine-slice tests.

Exact XP fidelity:

- [ ] Luna Blue active/inactive titlebar, window icon and system menu.
- [ ] Minimize, maximize/restore and close buttons: normal/hover/pressed/disabled.
- [ ] Titlebar drag, double-click maximize/restore, border/corner resize.
- [ ] Minimize with focus transfer and taskbar restore.
- [ ] Maximize to work area and restore exact normal geometry.
- [ ] Fullscreen transitions and return to previous state validated.
- [ ] Modal/owned windows, stacking, always-on-top and focus constraints.
- [ ] Taskbar activation/minimization rules; urgency/flashing behavior.
- [ ] Alt+Tab ordering, switching UI, reverse cycle and cancellation.
- [x] Alt+Tab and Alt+Shift+Tab focus cycling (overlay UI and cancellation remain pending).
- [ ] Initial placement, minimum sizes, multi-monitor and offscreen recovery.
- [ ] Wayland SSD negotiation and GTK CSD limitations addressed.

## Desktop and shell

- [x] Taskbar prototype: task list, activation/minimization, clock, reserved work area.
- [x] Standard foreign-toplevel protocol integration tested with real clients.
- [x] WM Mode / Floating Mode with saved geometry (personal additional feature).
- [x] Original Luna taskbar, task-button, Start background and tray image integration.
- [x] Start launcher prototype: visible application discovery and menu structure tested.
- [x] Two-column Start-menu structure and five source-indexed Luna panel backgrounds.
- [x] Run prototype: program/argument launch, error retention, singleton and cancel/reopen tests.
- [ ] Run Browse interaction, physical keyboard input and focus restoration verified.
- [ ] Start-panel rendered geometry, pinned/recent lists, icons and session footer fidelity.
- [ ] Interactive Start popup placement, keyboard dismissal and application launch verified.
- [x] Headless pointer-driven Start opening, outside dismissal, reopening and Run activation.
- [x] Headless Escape dismissal of Start/Run and keyboard focus restoration to a real application.
- [x] Home/End, All Programs Right/Left, cross-column navigation and Enter-to-Run integration test.
- [x] All Programs mapped-submenu Home/Up/Down/Enter traversal and controlled fixture launch.
- [x] Start button visual states, composited Windows logo and button-driven menu opening.
- [x] Start footer session dialog with explicit Stand By, Turn Off, Restart and Log Off actions.
- [x] Separate Log Off Windows confirmation and full-screen logoff transition prototype.
- [x] Full-screen XP Welcome/logon layer surface with source-indexed LogonUI account tile.
- [ ] Full application-list traversal, disabled-item navigation and XP wrapping rules verified.

### Original shell resource integration

- [x] Native 16/32px shell32 GROUP_ICON derivatives with component hashes and language IDs.
- [x] Desktop uses original My Documents, My Computer, Network Places, Recycle Bin and Internet Explorer artwork.
- [x] Start uses Explorer BITMAP 143 (25×20 flag), separate from the Luna background strip.
- [x] Start user header uses the extracted LogonUI account tile (114, 48×48) and XP Tahoma metrics.
- [x] Menu icons render with Cairo independently of the host GTK icon theme; empty application slots have no icons.
- [x] XP shell link ordering and extracted icons for Documents/Recent/Pictures/Music/Computer/Network/Printers/Control Panel/Help/Search/Run.
- [ ] Exact menu row geometry, account-picture frame and persistent pinned/recent policy.
- [ ] Correct per-application task/menu icons (currently the original generic XP application icon).
- [ ] Exact Show Desktop artwork, logoff icon mapping and desktop selection treatment.

Full XP fidelity remains pending:

- [ ] Taskbar size, placement, grouping and context menus.
- [ ] Exact Start button states and pinned/recent two-column policy.
- [ ] Notification area, hidden icons, clock and tooltips.
- [ ] Exact Quick Launch ordering, icons and launching policy.
- [ ] Exact wallpaper modes, desktop icon grid, selection and rename.
- [x] Bliss wallpaper and five-icon desktop prototype with selection/double-click plumbing.
- [x] Desktop context menu with Refresh/Home and wallpaper-mode Properties panel.
- [x] Quick Launch prototype: Show Desktop and Home actions in the taskbar.
- [ ] XP context menus, keyboard navigation and dismissal.
- [ ] Run dialog history, Browse behavior and XP application launcher policy.
- [ ] Exact shutdown/logoff artwork, multi-account logon and real PAM integration.
- [x] Shutdown/logoff dialogs, screen transitions and dry-run action tests.
- [ ] Full Display Properties/settings panels and file manager integration.
- [ ] Session startup/autostart and shell recovery.
- [x] XP WAV/WA_ extraction with provenance and optional external-player event hooks.
- [ ] Exact XP mixer volume, overlapping-event policy and complete System Events mapping.

## Measurement ledger — all values pending reference evidence

Record value, units, theme, DPI, font, source resource or screenshot, and tested
rendering alongside each entry. Use identical capture settings for comparisons.

| Measurement | Status |
| --- | --- |
| Titlebar height, caption buttons, border thickness | TODO |
| Taskbar height, Start button dimensions | TODO |
| Menu padding, separators, shadows | TODO |
| Icon spacing, desktop grid, label layout | TODO |
| Scrollbars and arrow buttons | TODO |
| Button padding, tabs, text fields, dialog margins | TODO |
| Font metrics, baselines and rasterization | TODO |
| System/disabled colors, hover/pressed states | TODO |
| Focus rectangles and keyboard cues | TODO |

## Assets and themes

- [x] ISO/directory extraction tool without code decompilation (tested on supplied ISO).
- [x] Index source/type/ID/language/format and known image metadata; unknown fields explicit.
- [x] Preserve grouped icon/cursor links and original resource bytes; hash verification.
- [ ] Luna Blue resource mappings and GTK3 theme.
- [ ] Screenshot comparisons at reference DPI and state-by-state behavior checks.
- [ ] Luna Olive.
- [ ] Luna Silver.
- [ ] Windows Classic.
- [ ] Royale (optional).
