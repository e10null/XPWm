# XP resource extraction

Requirements: Python 3.11+, 7z, wrestool (icoutils); ImageMagick for --previews.
The tools read PE data resources, never execute or decompile Windows programs.
No mount, root access or network is needed.

After extraction/verification, prepare_luna.py produces the source-indexed PNG
strips and optional process-local fonts consumed by the compositor and taskbar.
See [Luna rendering](../docs/LUNA_RENDERING.md) for its command and limitations.
These derivatives are separate from the unchanged raw resources and previews.
The desktop accepts the original Bliss JPEG and, when available, a sibling
`bliss.png` derivative. The PNG path uses Cairo directly, so headless tests do
not depend on the host's optional GdkPixbuf/Glycin image-loader data.

```
python3 tools/xp_assets.py YOUR_XP.iso --output assets/local/my-xp --profile all --previews
python3 tools/verify_assets.py assets/local/my-xp
make check-assets
```

The default profile is luna. Use all for the core shell libraries, CPL files,
cursors, sounds and bitmap/JPEG files. A Windows installation directory or
previously extracted directory can replace the ISO argument. Existing output
directories are refused so originals and later annotations cannot be overwritten.
Input files above 64 MiB in archives are skipped; this tool targets UI assets.

The current full extraction is assets/local/xp-sp3/. Open gallery.html in a
browser to inspect bitmap previews. index.json is the resource ledger;
theme-references.json records decoded theme properties with section/line
provenance and candidate bitmap links. No renderer defaults are inferred.

Output layout:

```
sources/I386/LUNA.MS_                         # Original compressed ISO member
sources/I386/LUNA.MS_.expanded/luna.mst        # Exact cabinet member name
raw/I386/LUNA.MS_.expanded/luna.mst/id-2/      # Numeric PE type 2 = BITMAP
  name-BLUE_STARTBUTTON_BMP/id-1033-entry-5388.dib
previews/...                                # BMP/PNG and decoded UTF-8 text
inventories/...wrestool.txt                   # Independent inventories
index.json
theme-references.json
gallery.html
```

Integer and string IDs use separate path prefixes. Directory entry offsets
disambiguate resources, and numeric types retain human-readable names in JSON.
Source paths distinguish identically named DLLs in different ISO directories.
The index records ISO/source/resource SHA-256 hashes, file offset, RVA, resource
type, ID/name, language, code page, original bytes and known image metadata.
GROUP_ICON/GROUP_CURSOR records link every component at the same language;
missing links are explicit and fail verification. Standalone CUR/ICO files
retain variant dimensions/hotspots; WAV headers provide sample format; ANI
headers provide frame count and timing. Unknown metadata is null.

PE traversal follows Microsoft's resource-directory specification:
https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#the-rsrc-section
It handles PE32 and PE32+, bounds-checks ranges, and rejects cycles. wrestool
independently checks resource counts. Its Luna listing repeats TEXTFILE as the
name of multiple resources, so we read full UTF-16 directory names directly.

Raw BITMAP resources are DIBs, not BMP files. BMP derivatives add only the file
header; verification checks the DIB bytes are unchanged. PNGs are viewing aids,
not yet rendering assets. Alpha interpretation of BI_RGB 32-bit data remains
unknown, and no magenta or other transparency key is guessed. Theme properties
must supply the rendering interpretation. Animated cursor frame decoding and
reconstruction of grouped PE resources into ICO/CUR files remain follow-up work.

Failure returns a nonzero exit code and records errors in index.json. Keep an
incomplete output for diagnosis and use a new directory to retry. Installation
destination paths are not reconstructed from setup INF files; original archive
paths are preserved. The all profile includes setup artwork too, so wallpaper
and sound-role classification still needs identification.
