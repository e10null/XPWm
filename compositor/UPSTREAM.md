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
