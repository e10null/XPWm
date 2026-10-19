# Upstream provenance

Source: https://codeberg.org/dwl/dwl
Tag: v0.7
Commit: 74e45c4014ae7048ecbb76eb6f54034b8b479480
Target: wlroots 0.18 (tested locally with 0.18.3).

Imported source, protocol XML, build files, manual, README, changelog and all
LICENSE files are retained. No upstream Git metadata is vendored.
Local changes: click-to-focus and floating monitor layout in config.def.h;
personal Hyprland bindings and keybind-actions.h; dwl.c scroll bindings and
volume-only key repeat; Makefile tracks/packages the action header.
The executable remains named dwl to keep the initial upstream diff small.

Milestone 2 adds decoration.c/.h and decoration-actions.h, Cairo/Pango build
dependencies, caption-aware geometry/hit-testing, minimize/maximize state and
request handling. client.h translates decorated client coordinates. The original
upstream tiling functions remain, including currently unbound functions that
produce unused-function compiler warnings. See ../docs/DECORATIONS.md.

Taskbar/mode increment: foreign-toplevel.h exposes window state/actions using
wlroots' standard manager; keybind-actions.h saves floating geometry for mode
switches. Layout labels now read WM Mode and Floating Mode.

## Selective upstream backports

The base remains dwl v0.7 / wlroots 0.18. The following upstream fixes are
adapted locally rather than importing the newer wlroots API migration:

- [PR #1108](https://codeberg.org/dwl/dwl/pulls/1108): filter consumed compositor
  shortcut keys from keyboard enter events and suppress their releases. XPWm
  retains its existing binding interface, Alt+Tab/system-menu handling and
  volume-only/Alt+Tab repeat policy. Keys are marked before dispatch so focus
  changes inside a binding cannot leak the triggering key; overlay-handled
  releases also clear the consumed state. Out-of-range keycodes are guarded.
- [8ad2c920](https://codeberg.org/dwl/dwl/commit/8ad2c920db7e2f1cb99709a8e46c968c5f526dc8):
  clear pointer focus and recompute it on move/resize release, allowing the
  client to restore its cursor instead of leaving the default cursor.
- [d1ebf3c9](https://codeberg.org/dwl/dwl/commit/d1ebf3c9855ec643c877a2216eaad21e179cd447):
  send tiled edges in the initial configure before Firefox chooses its window
  silhouette. XPWm additionally checks the monitor layout, so Floating Mode
  sends no tiled edges and keeps floating startup geometry.

The switcher regression observes actual keyboard key/enter and pointer enter
protocol events, including ordinary typing and overlay release cleanup. The
mixed-app test observes first-configure tiled edges as well as later layout
transitions. A transient whose parent is declared after its first configure
receives its floating state when mapped. Optional real Firefox coverage runs
with `XPWM_LAYOUT_FIREFOX=1`; add `XPWM_LAYOUT_START_TILED=1` to test tiled startup.
