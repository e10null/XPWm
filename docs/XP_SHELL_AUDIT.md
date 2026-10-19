# XP shell comparison — 2026-10-18

This audit compares the current XPWm source with public shell documentation,
original resources extracted from the supplied XP SP3 ISO, and the public ReactOS
compatibility implementation. ReactOS is a separate project, not Microsoft XP
source. No leaked Microsoft source was consulted or copied. This is a shell
comparison, not a Windows kernel or Win32 binary-compatibility implementation.

## References

- [Microsoft taskbar documentation](https://learn.microsoft.com/en-us/windows/win32/shell/taskbar): classic shell components, task buttons, notification callbacks and XP-specific settings.
- [Microsoft ToggleDesktop](https://learn.microsoft.com/en-us/windows/win32/shell/shell-toggledesktop): desktop/window toggle behavior.
- [Microsoft recent-document API](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shaddtorecentdocs): recently opened documents.
- [Microsoft window features](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features): window menus and system commands.
- [Public ReactOS Run dialog implementation](https://github.com/reactos/reactos/blob/master/dll/win32/shell32/dialogs/dialogs.cpp): compatibility reference for the editable Run MRU list, not an XP source-code comparison.
- Local resource ledgers: `assets/local/xp-tray/`, `assets/local/xp-sp3/`, `assets/local/luna-blue-complete/`.

## Gaps addressed by this audit

| Area | Previous implementation | Current implementation and evidence |
| --- | --- | --- |
| Run history | Plain empty entry | Editable dropdown; 26 successful commands, deduplicated and persisted per user. `tests/run-history.c`, native taskbar test. |
| Run targets | Executables only despite the instruction text | Programs retain literal argv parsing; existing documents, folders and URIs use GIO associations. Classification tests in `tests/run-command.c`; actual private-association launches in `tests/run-associations.c`. |
| Recent Documents | Disabled item | Up to 15 existing local recent files, newest first; private/stale items excluded. Private fixtures in `tests/start-menu.c`. |
| Show Desktop | One-way minimize | Second invocation restores only windows minimized by this action and reactivates its previous window. Previously minimized windows stay minimized. Two-client protocol test. |
| Task-button menus | No right-click actions | Restore, Minimize, Maximize and Close, using real compositor requests and live task lifetime references. Protocol integration test. |
| Task icons | Generic icon for every application | Desktop-entry icon when resolvable; original generic XP artwork otherwise. |
| Taskbar grouping | Every window always had a separate button | Crowded bars combine windows with matching application IDs, largest applications first. Window-list selection and Restore/Minimize/Close Group use real compositor requests; grouping is a saved preference. Backend and real pointer/protocol tests. |
| Taskbar properties | No preferences | Native Properties with saved clock/Quick Launch visibility, grouping, on-top layer and auto-hide. Real compositor tests verify a 2px activation edge, pointer reveal and stable work area. |
| Alt+Tab | Immediate focus cycling | Visual MRU snapshot, reverse cycling, Alt-release commit, Escape cancellation, minimized restore and safe removal of closing windows. Native keyboard/protocol tests. |
| Caption menu | Caption buttons only | Icon click, title-bar right-click and Alt+Space expose real Restore/Move/Size/Minimize/Maximize/Close actions. Keyboard/pointer movement and resizing support Enter/commit and Escape/rollback. |
| Search | Start link opened Run | Filename wildcards, UTF-8 text content, size/date ranges, optional recursion/hidden items, Stop/partial results, sortable results and saved `.xpsearch` queries. Private backend and native GUI tests. |
| Desktop files | Five fixed shortcuts | Live Desktop folder, Ctrl/Shift and rubber selection, keyboard navigation, drag positioning, saved grid/arrangement, Rename, Cut/Copy/Paste, New Folder, confirmed trash and incoming file drops. Backend and real compositor pointer/keyboard tests. |
| Shell destinations | Computer/Network/Printers all opened `/` | Computer/network locations use their virtual URIs; printers use installed management tools or local CUPS. |

Recent documents follow GTK applications' shared recent-document store; XPWm
cannot infer files opened by applications that never report them. Run history
stores command text locally under the XDG configuration directory. Windows path
syntax and executable compatibility are not emulated.

## Existing features verified in the code

- Luna captions, borders, taskbar, Start artwork and original XP font loading.
- Category folders in All Programs and visible default browser/mail pins.
- Native Run, Welcome, logoff and explicit shutdown controls; quiet normal interaction.
- Original 16px notification-area resources, compact volume popup, tray menus and balloons.
- Wallpaper preview and guarded per-display mode/refresh/scaling with persistent confirmed settings.
- Floating placement, maximize/restore/minimize, modal focus boundaries and optional tiling.

These entries establish implemented behavior, not universal pixel identity.

## Remaining work, in priority order

1. Remaining taskbar preferences: lock/resize/reposition, urgency flashing,
   additional toolbars and editing Quick Launch entries.
2. Per-application compositor switcher icons and reference visual comparisons;
   taskbar Move/Size and arrangement require additional compositor control support.
3. Outgoing desktop file dragging, drag-to-folder moves, shortcut creation, file
   properties, conflict dialogs, undo and a native Recycle Bin browser.
4. Search indexing and formatted-document content support, editable Start pins,
   recent-application policy and a complete settings hub.
5. Third-party notification-area registration, hidden/inactive icons and preferences.
6. Themes/Appearance/Screen Saver tabs, remaining Luna variants, and full settings dialogs.
7. Shell recovery/autostart, authenticated multi-user switching and session locking integration.
8. Toolkit/CSD/XWayland differences, accessibility, physical-device and mixed-DPI checks,
   plus reference-DPI visual comparisons against an actual XP installation.

Linux applications and services remain the backend for files, printers, network,
audio and advanced settings. Compatibility with all XP built-in applications,
Win32 APIs, drivers and system services is outside the present Wayland-shell scope.

## Follow-up fixes

Display dropdowns use a single-column below-field anchor with a field-width list;
compositor constraints include decoration and xdg geometry offsets. A real pointer
regression opens the refresh list next to an output edge and checks its position,
width and containment before running Keep/Revert and rollback tests.

Tray extraction explicitly picks the highest-depth native 16px frame. This fixes
the yellow legacy speaker variant while preserving original XP artwork. Connected
LANs use XP idle/receive/transmit/both-monitor frames driven by per-interface
traffic counters; lack of Internet access no longer displays a red question mark.

Search traverses local folders without following directory symlinks, skips hidden
items unless selected, and caps results at 1,000 with an explicit limit message.
It runs in a cancellable worker; closing the window cancels safely. Text-content matching reads regular UTF-8 files up to 16 MiB; binary files,
symlinks and larger files are skipped and counted. Size ranges use KiB; modified
date ranges include both dates. Saved queries store options and require an
explicit Search after opening. XP's indexing service, formatted-document filters
and Search Companion animation remain unimplemented.

## Taskbar and window-management pass

Right-click an empty part of the taskbar and choose Properties. Apply commits both
live state and `~/.config/xpwm/taskbar.ini` (or the XDG configuration directory);
Cancel discards changes since the last Apply. Defaults preserve the existing
visible clock/Quick Launch and on-top taskbar. Auto-hide is off by default, keeps
a 2px activation edge, delays hiding by 600ms, and stays open for its menus and
Properties. With auto-hide enabled, revealing it does not resize maximized apps.

Alt+Tab snapshots the active workspace's managed windows on the current monitor,
including minimized windows. Tab/Shift+Tab select, Alt release or Enter commits,
and Escape cancels without reordering focus. Closing candidates are removed safely;
locking, external focus changes or an exclusive shell panel cancel the overlay. The compositor switcher
currently uses original XP generic application artwork, as captions do; taskbar
buttons still resolve application icons independently.

The caption menu disables unavailable restore/maximize/resize operations, including
fixed-size dialogs. Move/Size accept pointer motion or arrow keys (8px; Ctrl for
1px), Enter or a click to commit, and Escape to restore the original geometry.
Double-clicking the caption icon closes its window. No interaction sounds were added.

## Desktop file pass

The desktop watches the configured XDG Desktop folder and keeps its five shell
shortcuts. Ctrl/Shift selection, background rubber selection, keyboard navigation,
F2, Delete, Ctrl+C/X/V and saved positions are supported. Arrange Icons By offers
name, size, type and modification date plus Auto Arrange and Align to Grid.
External URI drops copy local files into Desktop or a real folder icon; internal
drags reposition icons. Files open through their Linux associations.

File jobs run off the UI thread with cancellation. Rename rejects occupied names;
copy/paste generates unused names without overwriting. Recursive copies preserve
symlinks and reject destinations inside the source, including directory aliases.
Delete asks first and uses the system trash, with no permanent-delete fallback.
Cancellation or an error can leave completed items and a partial copied folder;
there is no transactional rollback or undo. The Recycle Bin shortcut uses the
installed file manager. Layout is saved in `$XDG_CONFIG_HOME/xpwm/desktop.ini`.

Native dialogs return keyboard focus to their originating desktop surface.
Initial xdg configuration no longer steals focus before a new window maps.
`tests/desktop-files.c` covers backend operations and boundary cases;
`tests/desktop-input.c` exercises real compositor input with private Desktop,
clipboard, layout and data directories.

## Crowded taskbar grouping

Group similar taskbar buttons is enabled by default and is configurable through
Taskbar Properties. When separate buttons would have less than 100 logical pixels
each, the shell combines the largest applications until the buttons fit or no
matching applications remain. Window order within a group and the first window's
position remain stable. Wider displays expand the groups automatically. This is
XPWm's explicit grouping policy, not a claim that every XP registry policy is
emulated.

The grouped button shows its window count, application name/icon, active state
and a dropdown arrow. Left-click opens the individual window list; the active
window is bold, and choosing a minimized member restores it. Right-click offers
Restore Group, Minimize Group and Close Group. Close sends normal close requests,
so applications still control unsaved-document prompts. Closing a window dismisses
an open task menu; menu snapshots retain safe references through activation.

Groups use exact, nonempty Wayland application IDs (or the compositor's XWayland
application identifier). Unknown or different IDs stay separate. The freedesktop
desktop entry supplies the display name/icon when available. Grouping does not
supply missing application IDs or implement Windows process/registry grouping.
`tests/taskbar-groups.c` covers thresholds, order, unknown IDs and persisted
preferences. `tests/taskbar-groups-state.c` and the private shell harness verify
pointer-opened menus and real group actions at 640×480 while preserving an
unrelated window.

## Desktop drag rendering

Icon motion invalidates the old and new icon rectangles; rubber selection
invalidates its previous/current bounds and icons whose selection changes. GTK
coalesces these regions into the next frame. Unchanged motion does not request
a redraw. Icon positions are persisted on release, not during motion.

Wallpaper resampling is cached per desktop size, mode and device scale. Each icon
keeps one small transparent image of its current icon/label/selection state,
rebuilt when selection or device scale changes; folder reloads release old tiles.
Selection tint buffers are clipped to icon bounds. At 100% scale each tile uses
about 26 KiB, and a 1920×1080 wallpaper cache uses about 8 MiB. This replaces
repeated full-screen temporary tint buffers and repeated text/background work.

The local Cairo stress fixture (1080p, 32 selected icons, warm assets) changed
from roughly 105 ms per full repaint to roughly 0.7 ms; a small damaged-area
repaint is below 0.1 ms. These are renderer measurements, not end-to-end input
latency or guarantees for other GPUs/displays. `tests/desktop-render.c` checks
incremental drag, overlapping labels and shrinking bands against full-redraw
pixels across wallpaper modes and 100%/200% scales. Native pointer tests also
verify that dragging paints a partial frame before release.
