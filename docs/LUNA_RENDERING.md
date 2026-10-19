# Local Luna Blue rendering

The compositor and taskbar now share a small Cairo strip renderer in
shared/luna.c. It reads theme.ini and PNGs from assets/local/luna-blue-complete,
falling back to assets/local/luna-blue-start and then luna-blue,
relative to the launch directory.
XPWM_THEME_DIR explicitly overrides that choice. For installed
launchers, set XPWM_THEME_DIR to an absolute path. Missing images fall back to
recreated drawing. No Windows resources or fonts are distributed with the code.

## Prepare from an extraction

Run from the repository root after tools/xp_assets.py and verify_assets.py:

```
python3 tools/prepare_luna.py assets/local/xp-sp3 \
  --output assets/local/luna-blue-complete \
  --iso en_windows_xp_professional_with_service_pack_3_x86_cd_x14-80428.iso
make tty
```

This directory is already prepared in the current workspace. The tool refuses
to overwrite it: choose a new output directory for subsequent preparations and
select it with XPWM_THEME_DIR. The optional ISO argument extracts Trebuchet,
Tahoma and Franklin Gothic Medium regular/bold/italic faces needed here. Fonts
are registered only inside XPWm processes, not installed into the host desktop.

Every derivative retains its bitmap resource name. provenance.json records the
source, resource ID, language, original hash, raw path, theme property/line and
conversion. Original extraction files and the ISO remain unchanged.

## Interpretation

NORMALBLUE_INI supplies image counts, sizing margins and tile/stretch mode.
The twenty roles cover active/inactive captions and three frame edges, five
push-button states, eight caption-button states,
four glyph strips, Start background, taskbar, task buttons, tray background and
five Start-panel backgrounds (header, programs, more programs, places and footer).
Caption buttons use a 21-pixel background and 13-pixel glyph selected to fit the
content box. States are zero-based internally; active normal/hot/pressed/disabled
precede their inactive variants. Task buttons use checked/hot-checked for active
windows. Start uses normal/hot/pressed states, including pressed while its menu is open.

The converter handles uncompressed 8/24/32-bit DIBs, palette lookup, row direction,
and color-key transparency. Tray transparency is red, not the default magenta.
The mapped 32-bit Luna images retain straight BGRA alpha into RGBA PNG; Cairo
premultiplies on load. This interpretation is specific to these strips, not a
general promise that every BI_RGB resource has meaningful alpha.

Nine-slice rendering preserves margins when space permits, tiles the requested
regions and uses nearest-neighbor sampling. Tiny targets compress margins to fit.
Pango text is rendered at 96 DPI with the theme's requested fonts and caption/Start
shadows. Linux font rasterization is not claimed identical to XP GDI.

State-order reference: [Microsoft parts and states](https://learn.microsoft.com/en-us/windows/win32/controls/parts-and-states).
Size-based image selection was cross-checked against
[Wine's theme drawing implementation](https://github.com/wine-mirror/wine/blob/master/dlls/uxtheme/draw.c).

## Verification and remaining work

make check includes license-free conversion fixtures and a synthetic Cairo test
for states, nine-slice corners, tiling, invalid manifests and tiny targets, plus
headless native-client/window/taskbar integration. Test the fallback with:

```
XPWM_THEME_DIR=/nonexistent/xpwm-theme make check
```

The real-resource headless screenshot confirms composition, not 1:1 XP fidelity.
Caption height 26, outer border 3 and taskbar height 30 remain provisional. Source
strip heights are not automatically final control heights. Resource-backed side/bottom frames and the full-width transparent caption
silhouette are now implemented. Actual application icons, exact XP menu layout, disabled
button policy and DPI/reference comparisons remain TODO. Window-management modes
and personal shortcuts are unchanged by this rendering milestone.

## Native dialog controls

`shell/xp-style.c` applies a scoped GTK provider to shell-owned dialogs and
context/submenus. Push buttons draw the original five-state Luna bitmap before
GTK draws their labels, mnemonics and focus rectangles. Entries use the XP
sunken blue-gray border, white fill and Tahoma text. Missing resources retain a
Cairo fallback. No host GTK configuration is changed.

The caption bitmap spans the whole window width and includes the outer top
edge, preserving its transparent corners. Three narrow scene buffers render
active/inactive left, right and bottom strips; fullscreen hides all four
surfaces. Caption input retains the existing logical geometry.

Display Properties has a Desktop tab with a CRT preview and working wallpaper
position controls. Apply commits the selection; Cancel discards subsequent
unapplied edits. Other XP Display Properties tabs are not implemented. Desktop
selection uses a blue tint on the resource icon and a tight text highlight,
replacing the previous large solid selection rectangle.

Capture the current rendering on a private headless output with:

```
XPWM_MENU_SCREENSHOT=/tmp/xpwm-start.png \
XPWM_RUN_SCREENSHOT=/tmp/xpwm-run.png sh tests/shell-ui.sh
XPWM_PROPERTIES_SCREENSHOT=/tmp/xpwm-properties.png \
XPWM_DESKTOP_SCREENSHOT=/tmp/xpwm-desktop.png sh tests/desktop.sh
```

These screenshots establish native composition and support visual review.
They do not establish pixel equivalence to XP: GDI rasterization, application
content, complete shell menus, tray services and several control policies still
differ. `XPWM_THEME_DIR` can select an older prepared theme or force fallbacks.
