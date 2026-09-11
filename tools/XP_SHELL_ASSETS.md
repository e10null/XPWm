# Private XP shell artwork

The running shell uses locally extracted resources, not a downloadable icon theme.
From the repository root, after creating the XP PE extraction index:

```sh
python3 tools/prepare_icons.py assets/local/xp-sp3 assets/local/xp-icons
python3 tools/prepare_shell_bitmaps.py
make
```

Icon conversion requires Pillow. It reconstructs ICO frames from GROUP_ICON
component links, verifies the component SHA-256, and selects the highest-depth
native 16, 32 and 48 pixel variants. It refuses an existing output directory.
Library, group ID, language and size remain in each path; index.json retains
the source component and hash. Contact sheets allow visual identification.
The bitmap helper extracts Explorer BITMAP 143 at its native 25×20 size and the
LogonUI account-tile candidates (including 114 at 48×48), using the original
magenta transparency key; changed existing images are not overwritten.

Runtime uses Cairo PNG decoding rather than the host icon theme or GTK image
loaders. XPWM_ASSET_ROOT can point to the project root when needed. These files
remain private under assets/local and must not be included in public releases.
Missing artwork still uses the prototype fallback; that is not XP fidelity.

Current shell32 mappings: 16 Computer, 17 Printers, 18 Network Places, 20 Control
Panel, 21 Recent Documents, 23 Search, 24 Help, 25 Run, 28 power, 32 empty
Recycle Bin, 34 All Programs, 35 Log Off, 235 Documents, 236 Pictures, 237
Music, 238 Videos, 242 Show Desktop, and 512 Internet Explorer. Generic
application artwork is group 3; application-specific icon resolution, persistent
MRU storage and precise menu geometry remain TODO.
