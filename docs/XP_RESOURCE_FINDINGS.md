# Initial XP SP3 resource findings

Source: the provided XP Professional SP3 x86 ISO, I386/LUNA.MS_. The cabinet
member is named luna.mst; its extracted SHA-256 is
dd8e86e5a7c19ea9fe0e34992babefe592055d68eb8659763c8300558ca5dcb4.
This is the Luna style data normally associated with Luna.msstyles. We retain
the original cabinet member name rather than renaming it silently.

Resource language: 1033. Theme settings below come from the TEXTFILE resource
NORMALBLUE_INI, with source lines in the generated theme-references.json.
Each image is a BITMAP resource; the table records source bitmap dimensions,
not necessarily its final on-screen size.

| Resource | Source dimensions | Depth | Explicit theme properties |
| --- | --- | --- | --- |
| BLUE_FRAMECAPTION_BMP | 66 × 58 | 8 bpp | Window.Caption: 2 vertical images; sizing margins 28,35,9,17 |
| BLUE_CLOSEBUTTON_BMP | 21 × 168 | 32 bpp | Window.CloseButton: 8 vertical images; sizing margins 5,5,5,5 |
| BLUE_STARTBUTTON_BMP | 99 × 99 | 24 bpp | Start::Button: 3 vertical images; sizing margins 6,52,13,14 |
| BLUE_TASKBARBACKGROUND_BMP | 50 × 28 | 8 bpp | TaskBar.BackgroundBottom: 1 image, tile sizing |

The theme's SysMetrics section specifies:

- CaptionFont: Trebuchet MS, 10, bold; CaptionBarHeight: 25.
- SmallCaptionFont: Tahoma, 8, bold; SMCaptionBarHeight: 17.
- ScrollbarWidth/ScrollbarHeight: 17.
- MenuFont, StatusFont, MsgBoxFont and IconTitleFont: Tahoma, 8.
- ActiveCaption RGB: 0,84,227; InactiveCaption: 122,150,223.
- Background RGB: 0,78,152; Btnface: 236,233,216.

The Start button specifies Franklin Gothic Medium, 14, italic. Its extracted
background contains three green states, without text or the Windows logo. The
caption close button similarly uses separate background and glyph resources.
These are composited controls, not complete screenshots ready to paste on screen.

Resource-derived strip frame dimensions are 66×29 for the caption, 21×21 for
close-button backgrounds, and 99×33 for Start-button backgrounds. Do not treat
those as final control dimensions: sizing margins, content margins, DPI and
layout rules still determine rendering. Raw extraction previews retain magenta;
the separate rendering derivatives apply transparency from theme mappings.

NORMALMETALLIC_INI and NORMALHOMESTEAD_INI provide Silver and Olive settings;
large-font variants and theme lists are also preserved. Luna Blue caption/button
and taskbar resources are now integrated; other variants remain pending.

See LUNA_RENDERING.md for mappings, conversion and current limitations. Next:
compare placement and geometry against an XP reference at matching DPI and fonts.
Do not silently reinterpret uncertain values.
