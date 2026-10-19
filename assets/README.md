# Assets

Resources have been extracted locally from the user's ISO into local/xp-sp3/.
The ISO itself is unchanged. All extracted originals and derivatives remain
under the Git-ignored local/ directory; they are not part of the source tree's
distributable assets. Pilot extractions remain in local/xp-sp3-luna/ and
local/xp-sp3-first-pass/ for comparison.

Planned outputs: raw/, themes/luna-blue/, themes/luna-olive/,
themes/luna-silver/, themes/classic/, icons/, cursors/, sounds/, wallpapers/.
local/xp-sp3/index.json preserves source identity and derivative links.
local/xp-sp3/gallery.html previews bitmaps; theme-references.json records theme
properties. See ../tools/README.md for regeneration and verification, and
../docs/XP_RESOURCE_FINDINGS.md for initial measured resource facts.

`tools/prepare_sounds.py` writes local `sounds/` derivatives and an index that
preserves each XP Media source filename, compressed/uncompressed format and WAV
metadata. The raw media and canonical event files remain Git-ignored.
