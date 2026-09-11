# XPWm

A small C/dwl/wlroots Wayland desktop project targeting Windows XP SP3 behavior
and visual fidelity, starting with Luna Blue. The runnable dwl baseline now has
compositor-drawn titlebars, caption buttons and basic window actions, with
floating layout and click-to-focus. The GTK3/Cairo taskbar runs as a separate
shell process. Locally extracted Luna Blue images now render captions, caption
buttons and the taskbar, with process-local XP fonts. Recreated artwork remains
the fallback when assets are unavailable. The supplied XP ISO is unchanged.
The separate desktop layer loads the extracted Bliss wallpaper (with a Cairo
PNG path that remains usable in minimal/headless environments) and an XP-style
icon column. The green taskbar Start button opens the two-column Start menu;
there is no hidden global menu shortcut.
See [Luna rendering](docs/LUNA_RENDERING.md) for preparation and remaining gaps.

## Build and check

Dependencies: C compiler, make, pkg-config, wlroots **0.18.x** development files,
Wayland development files and scanner, wayland-protocols, libinput, xkbcommon,
Cairo, PangoCairo, Fontconfig and libdrm development files. The taskbar additionally needs
GTK3, gtk-layer-shell and wayland-client development files.
Use the pinned upstream version; newer wlroots APIs are not interchangeable.

```
make
make check
```

The checks also require wayland-info, timeout and foot. The smoke check launches a private
headless compositor using pixman, connects a native Wayland client, checks output,
xdg-shell and layer-shell globals, and shuts down. This is not a visual or input
fidelity test. A second integration check runs a real foot client and exercises
caption hit-testing, active/inactive rendering state, button cancellation,
maximize/restore/fullscreen, minimize/recovery, left-edge resize, titlebar move,
and close/unmap cleanup. Input is injected into compositor handlers; physical
input devices and a real multi-monitor session are not covered.

Optional screenshot from that private test display (requires grim):

```
make check XPWM_TEST_SCREENSHOT=/tmp/xpwm-decorations.png
```

Drag the titlebar to move, double-click it to maximize/restore, or drag borders
to resize. Caption buttons minimize, maximize/restore, and close. Click a taskbar
button to activate/restore its window, or minimize it if already active.
Super+Tab also cycles/restores windows on the current workspace.
Start -> Run opens a native program-launch dialog with quoted arguments,
Browse and inline launch errors. Its styling and command semantics are not yet
XP-exact; see [shell notes](shell/README.md).
See [decoration notes](docs/DECORATIONS.md) for provisional metrics
and remaining limitations.

From an existing Wayland session, launch an isolated nested desktop:

```
make nested
```

It starts the Bliss wallpaper/icon desktop, taskbar and a native foot test window.
If the project is launched outside its source directory, set `XPWM_ASSET_ROOT` to
the repository root (the binaries also discover a sibling `assets/` tree automatically).
See [TESTING.md](TESTING.md) for the manual,
headless, VT, VM, Podman and VNC workflows.

For an isolated real-seat test from a logged-in text TTY, run `make tty`.

From a suitable local VT with seat/device permissions:

```
./compositor/dwl -s foot
```

This does not install anything or replace your current desktop. foot is the
terminal for the VT example. Bindings now follow your Hyprland configuration:
Super+Return opens kitty; Super+E opens thunar; Super+D opens the configured
launcher; Super+left/right drag moves/resizes windows; Super+F toggles fullscreen;
Super+Shift+Q closes a window; Super+Shift+M exits. Super+Shift+Space toggles
WM Mode/Floating Mode. WM Mode tiles ordinary windows, including windows
previously dragged into floating state. Floating Mode restores saved positions
and sizes. Dialogs, fullscreen and maximized windows are exempt. Modes are per
monitor and reset to Floating Mode at startup.
See [KEYBINDINGS.md](KEYBINDINGS.md) for the complete
mapping and pending Hyprland-specific features. In nested mode the host may
intercept keys. These personal bindings take precedence over XP shortcut fidelity.

Edit compositor/config.h for local bindings; it is copied from config.def.h
on the first build and is not overwritten thereafter. If updating defaults,
merge changes into your existing config.h before rebuilding.

Optional XWayland build (requires xcb, xcb-icccm and runtime Xwayland):

```
make clean
make XWAYLAND=-DXWAYLAND XLIBS='xcb xcb-icccm'
```

Clean when switching build flags. The default build is native Wayland only.
XWayland-enabled builds start the X server lazily for X11 clients.

See [ARCHITECTURE.md](ARCHITECTURE.md),
[XP_COMPATIBILITY.md](XP_COMPATIBILITY.md) and
[upstream provenance](compositor/UPSTREAM.md). compositor/ retains upstream
licenses; shell/, tools/ and assets/ describe the next boundaries.

Asset tooling: [extraction instructions](tools/README.md) and
[initial XP resource findings](docs/XP_RESOURCE_FINDINGS.md).

## Source-only publication

The public source tree intentionally excludes the Windows XP ISO, extracted
Microsoft artwork/fonts and generated binaries. They are ignored by
`.gitignore`. To reproduce the visual shell, provide your own XP SP3 media,
run the extraction tools described in [tools/README.md](tools/README.md), and
prepare the local Luna/icon derivatives. The source, tests and provenance
format are intended to be shareable; the private assets are not.
