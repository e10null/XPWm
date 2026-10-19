# Luna application theme

`make tty` and `make nested` use `tools/start-session.sh` to source
`tools/session-theme.sh` **before starting the compositor**. Apps launched by
Super+Return/Super+E and other compositor shortcuts therefore inherit Luna too,
as do apps launched by Start/Run and the shell. Welcome, taskbar and desktop
receive the same session defaults. Newly started GTK3 applications
inherit `GTK_THEME=XPWm-Luna`, local XP fonts and a source-indexed XP icon theme.
GTK2 receives a matching palette/font theme and original push-button images via
`GTK2_RC_FILES`; GTK2 applications require the optional XWayland build. The host's
desktop theme, GTK files, icon settings and installed font directories are unchanged.

The source-only fallback lives in `themes/XPWm-Luna`. When the private XP
extraction exists, `make` runs `tools/prepare_app_theme.py`. It verifies the
original bitmap hashes and generates native-size state frames under
`assets/local/share/themes/XPWm-Luna`. These include push buttons, checkboxes,
radio buttons, combo backgrounds, tabs, scrollbars, sliders and progress bars.
The private provenance ledger records each source resource, hash, frame and
NORMALBLUE_INI line. Folder/document/drive/action icon aliases are generated
under `assets/local/share/icons/XPWm-Luna`, with their own provenance ledger.
Unknown icons inherit Adwaita/hicolor.

Tahoma and the other extracted fonts are exposed to child processes through a
private Fontconfig configuration. `XDG_DATA_DIRS` exposes the generated theme,
icons and Theme Preview launcher. `GTK3_MODULES` loads the small
`shell/xpwm-luna-module.so` only in GTK3 applications. That module places the Luna
CSS above host user CSS, which can otherwise repaint controls even when
`GTK_THEME` selects Luna. Shell-specific screen/dialog styling retains its own
higher priority. The module also sets `gtk-font-name` to Tahoma 8 and uses
full hinting with grayscale antialiasing, so host font preferences cannot change
the control metrics or introduce colored subpixel fringes. Display DPI and output
scaling remain configurable. No global GTK module is installed, and `GTK_MODULES`
is unchanged.

Open **Start → All Programs → Luna Theme Preview** to inspect real GTK controls
and XP file icons. The preview does not attach a shell CSS provider: it inherits
the application theme in the same way as other GTK applications. To run one
program with the same theme from a terminal in the project directory:

```sh
. tools/session-theme.sh
./shell/xpwm-theme-preview
# Or start a new instance of another GTK3 application from this terminal.
```

Restart XPWm after rebuilding so the session inherits the new environment.
Applications already running in a different session can retain their previous
theme, including single-instance applications activated through D-Bus.

This currently covers GTK3 controls and a GTK2 compatibility theme. Qt, GTK4,
libadwaita, Electron and applications with custom rendering are not made
pixel-identical by a GTK theme. Content/layout still belongs to each application,
and Pango/FreeType rasterization differs from XP GDI. Further toolkit themes and
reference comparisons are needed before claiming a complete 1:1 desktop.

## Verification

`make check` launches the application preview through the compositor session
launcher, without sourcing the theme again in the client. It also includes
synthetic strip conversion/hash tests, native GTK CSS
parsing, inherited palette/font/icon assertions with conflicting host font settings and a headless app screenshot.
The Welcome input harness also tests overlapping interactive layer focus, which
is needed when the shutdown panel opens above Welcome.

```sh
XPWM_THEME_SCREENSHOT=/tmp/xpwm-luna-app.png sh tests/app-theme.sh
XPWM_LOGON_SCREENSHOT=/tmp/xpwm-logon.png \
XPWM_WELCOME_SCREENSHOT=/tmp/xpwm-welcome.png \
XPWM_LOGOFF_SCREENSHOT=/tmp/xpwm-logoff.png sh tests/session-ui.sh
```

The application CSS test fails on new theme parse errors. Host GTK imports can
still print their existing parsing warnings while GTK initializes; they do not
supply the effective Luna widget styles.
