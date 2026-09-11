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

- Actual Luna caption/button strips are integrated; rounded outer silhouettes,
  side/bottom frame resources and exact placement/metrics remain pending.
- Application icons (currently a generic window icon), system menu and icon actions.
- Button availability/disabled visuals using application capabilities and size hints.
- XP modal-owner minimize/restore rules, stacking/focus details, always-on-top.
- Correct minimum/maximum size constraints for every client type.
- GTK applications that insist on CSD may show their own titlebars too. SSD-capable
  clients use the compositor frame. GTK theme/application integration remains pending.
- Fractional/HiDPI crispness: current buffers are rendered at logical resolution.
- XWayland builds compile; X11 runtime geometry/state behavior still needs testing.
- Real multi-monitor input, monitor removal, output scaling, taskbar work-area
  changes, popups, locks and pointer constraints need broader session testing.
- Maximized titlebar dragging currently leaves the window maximized; Super-drag
  restores first. Verify the intended XP behavior before changing it.

The automated test uses a real headless wlroots output and foot client. It checks
one edge resize and state transitions, not every edge, client or XP behavior.
Successful screenshot inspection establishes basic rendering, not pixel fidelity.
