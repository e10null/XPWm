# Decoration prototype — milestone 2

Implemented inside the existing dwl scene/window lifecycle:

- Cairo titlebar buffers with Pango text, active/inactive colors and a generic icon.
- Minimize, maximize/restore and close glyphs, hover/pressed visuals; releasing
  outside a caption button cancels the action.
- Caption drag, double-click maximize/restore, all border/corner resize directions.
- Separate normal and fullscreen restore geometry; maximize uses monitor work area.
- Minimized clients leave layout/focus selection and stay mapped. Super+Tab
  includes them and restores them, including when every window is minimized.
- Native maximize/minimize requests and optional XWayland state notifications.
- Client content offsets, clipping and hit-testing account for the titlebar.

decoration.c owns caption composition and wlroots buffer lifetime. shared/luna.c
loads local Luna strips and fonts; recreated artwork is the asset-free fallback.
decoration-actions.h contains the small window-state/input implementation.
dwl.c remains responsible for geometry, scene placement, focus and lifetime;
client.h translates client coordinates. No GTK dependency was added to dwl.
Pango handles UTF-8 and title ellipsizing; Cairo buffers are read-only to consumers.
Moving a window reuses its caption buffer; resizing its width redraws it.

## Provisional values — not XP measurements

Caption 26 logical pixels, border 3, caption buttons 21 with 2-pixel gaps,
minimum content width 120, generic icon 16. Double-click uses 500ms and a 4-pixel
movement tolerance. The resource-backed path requests Trebuchet MS Bold 10 at
96 DPI; fallback drawing requests Tahoma Bold 10, with system font fallback.
Every value must be verified against the user's XP SP3 reference/resources.
Geometry remains provisional even though the mapped artwork is now original.

## Remaining fidelity work

- Luna caption/button strips, rounded outer silhouettes and side/bottom frame
  resources are integrated; exact placement/metrics remain provisional.
- Application icons (currently a generic window icon), system menu and icon actions.
- Additional button capability policies beyond fixed-axis maximize handling.
- XP modal-owner minimize/restore rules, stacking/focus details, always-on-top.
- Broader X11 size-hint and increment/aspect-ratio validation.
- GTK applications that insist on CSD may show their own titlebars too. SSD-capable
  clients use the compositor frame. The session provides a GTK2/GTK3 Luna theme; other toolkits need separate support.
- Fractional/HiDPI crispness: current buffers are rendered at logical resolution.
- XWayland builds compile; X11 runtime geometry/state behavior still needs testing.
- Real multi-monitor input, monitor removal, output scaling, taskbar work-area
  changes, popups, locks and pointer constraints need broader session testing.
- Maximized titlebar dragging currently leaves the window maximized; Super-drag
  restores first. Verify the intended XP behavior before changing it.

The automated test uses a real headless wlroots output and foot client. It checks
one edge resize and state transitions, not every edge, client or XP behavior.
Successful screenshot inspection establishes basic rendering, not pixel fidelity.

## Window management behavior

Normal native Wayland windows now open inside the monitor work area and cascade
from the previously focused window. Dialogs and fixed-size windows are centered
on their owner, or the work area when no owner exists. Minimum and maximum client
sizes are applied before decoration extents; fixed axes cannot be border-resized.
The maximize button is disabled for windows with a fixed axis, and maximize
requests for those windows are ignored. When left/top resizing reaches a size
limit, the opposite edge stays anchored. Caption placement remains recoverable
when moving windows vertically beyond the work area.

Alt+F4 closes the focused application. It does not close a background application
when an interactive layer (Start, Welcome or a session confirmation) owns focus.
The existing Super+Shift+Q shortcut remains available. Floating is the XP default;
Super+Shift+Space still toggles tiling and restores saved floating positions.
System menus, full modal-owner policies and X11 runtime constraints remain work.

## Mixed application layout regression

The global Super+Shift+Space toggle includes maximized normal windows, saves
their floating geometry and maximize state, and restores both when returning
to Floating Mode. Dialogs and fixed-size transients retain floating behavior.
Normal tiled clients receive the actual tiled protocol state and move back to
the tile scene layer even if previously dragged into the floating layer. A
remembered startup maximize request does not remove a newly opened normal app
from a tiled layout.

Tiled slots take priority over client minimum/maximum size hints, preventing
large browser/GTK minimums from expanding over adjacent tiles. Floating windows
still honor their size hints. Apps that refuse a smaller configure are clipped
to their tile; their own content can require a larger tile to remain usable.
Configure bounds describe the available content work area rather than including
Luna decoration extents. Fractional nine-slice joins use hard clip edges so thin
frame strips do not acquire translucent seams.

The session requests system frames from cooperating GTK/Mozilla applications;
GTK decoration button layout is empty because XPWm supplies window controls.
An application that draws its own header/content can still retain that UI:
server decorations cannot rewrite arbitrary app content.

`make check` includes `tests/layout-state.sh` with a terminal, an oversized-minimum
GTK window and a fixed transient. For a real Firefox check with a disposable
profile and private D-Bus/config/cache directories:

```sh
make -C compositor layout-state-test
make -C shell layout-client-test
XPWM_LAYOUT_FIREFOX=1 sh tests/layout-state.sh
```

The harness checks nonoverlapping tiles, scene layers, tiled configure flags,
maximize restoration and floating configure flags. The optional browser check
requires Firefox and does not use an existing browser profile.
