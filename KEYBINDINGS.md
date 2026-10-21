# Personal bindings

Imported from ~/.config/hypr/keybinds.conf. This is a compiled snapshot, not a
live Hyprland parser. Both config.def.h and the current local config.h are updated.
No changes were made to the original Hyprland file.

| Shortcut | Action |
| --- | --- |
| Windows / Super (tap either key) | Toggle Start on release after a short tap (up to 250 ms); holds and other shortcuts cancel |
| Print / Super+Shift+S | Native screenshot: full desktop / select area; saves PNG and copies image |
| Super+Shift+F | Native area screenshot (same as Super+Shift+S) |
| Super+Shift+X | hyprpicker -a |
| Ctrl+Alt+L / Super+Shift+L | loginctl lock-session |
| Super+Return / Super+E | Command Prompt-style XFCE Terminal / thunar |
| Super+D / Super+R / Super+Shift+D | Native XP Run dialog |
| Super+Shift+B | Original pkill -HUP quickshell |
| Alt+F4 / Super+Shift+Q | Close focused window (preserves apps behind modal layers) |
| Super+Shift+M | XP logoff screen, then log off |
| Super+F / Super+Space | Fullscreen / per-window floating toggle (Floating Mode or dialogs) |
| Super+Shift+Space | WM Mode / Floating Mode, restoring saved floating geometry |
| Alt+Tab / Alt+Shift+Tab | Visual MRU switcher; release Alt to activate, Escape to cancel |
| Alt+Space | Window menu: Restore, Move, Size, Minimize, Maximize, Close |
| Super+Tab | Cycle workspace windows, restore minimized windows, and raise focus |
| Super+H/L/K/J | Focus nearest window center in that direction, same monitor |
| Super+Shift+arrows | Move window 20 logical pixels in Floating Mode |
| Super+Ctrl+arrows | Resize by 20 logical pixels in Floating Mode |
| Super+C | Center window in work area in Floating Mode |
| Super+1…9,0 | Workspaces 1…10 |
| Super+Shift+1…9,0 | Move to workspace 1…10 and follow |
| Super+Alt+Up/Down | Next/previous workspace, wrapping through 1…10 |
| Super+wheel down/up | Next/previous workspace |
| Super+A / Super+Shift+A | Toggle special tag overlay / send window to special tag |
| Super+left/right drag | Move / resize |
| Volume up/down | Change default sink volume by 5%, repeating on hold; volume up unmutes, capped at 100% |
| Audio mute / mic mute | Toggle the default output / input using wpctl |
| Brightness down/up | Change screen brightness by 10%, repeating on hold; brightnessctl or logind/elogind fallback |

Ctrl+Alt+F1…F12 VT switching remains available. Old conflicting dwl development
bindings were removed. Media shortcuts work while menus and Alt+Tab are open.

## Pending differences

- Super+P, Super+S, Super+Shift+P: dwindle pseudo, split and allpseudo are not
  implemented by dwl's tile layout. Reserved, currently unbound.
- Super+G and the duplicate Super+Tab group action: tab groups are not implemented.
  Super+Tab cycles ordinary windows. The duplicate group notification is omitted.
- WM Mode tiles ordinary windows even if previously dragged into floating state;
  dialogs and fullscreen/maximized windows are exempt. Floating Mode restores
  geometry saved when entering WM Mode. The mode applies to the selected monitor.
  Mode changes save automatically and restore for that monitor at the next login.
  An unknown output name uses the last saved mode.
  Directional focus uses center distance without monitor crossing.
- Keyboard moves use a 20-pixel step in Floating Mode; WM Mode blocks manual
  moves, resizing and per-window floating for ordinary windows. Dialogs retain
  their floating controls.
- The special workspace is a dwl tag overlay, without Hyprland's separate special
  workspace geometry/animation. Mode notifications are omitted.
- Executable/script dependencies are used as supplied, not installed. The locker
  command requires a session listener that launches a Wayland locker; this project
  does not yet provide one. Successful dispatch is not proof the screen locked.
- The retained quickshell reload command remains tied to your external shell. In a nested
  session they may affect that running shell. Centering is implemented locally
  instead of dispatching hyprctl to the host compositor.

Build and launch checks cover compilation and Wayland startup. Interactive
shortcut behavior and external applications still require a session-level check.

Super means the Windows key. Super+Shift+Space changes the entire monitor between
tiling and floating (overlapping) windows. Super+Space only changes the focused
window's floating state, which is useful while tiling. The terminal shortcut uses
a fresh XFCE Terminal window with black background, light text, hidden menus and
80×25 geometry; foot is the fallback when XFCE Terminal is unavailable.

In the caption menu, Move/Size use arrow keys (8px, or 1px with Ctrl), Enter to
commit and Escape to cancel. The menu also opens from the caption icon or a
right-click on the title bar.
