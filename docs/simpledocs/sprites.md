# Sprites (`src/engine/sprite.*`, `tools/sprite_bake/`, `assets/sprites/`)

**Format (`.sprite`, UTF-8 text):** `sprite <w> <h>`, then `palette` with `<key> #rrggbb` lines (key is one of `[A-Za-z0-9]`), then `pixels` with exactly `h` rows of exactly `w` characters (`.` is transparent, anything else must be a palette key). `#` lines and blank lines are ignored; CRLF is accepted. Width and height are 1-512.

**`parseSprite` / `loadSprite`** return `std::expected<Sprite, std::string>`. Errors name the line, for example `line 5: unknown palette key 'z'` or `line 6: row has 2 characters, expected 3`. Problems that have no line (a missing section, a wrong row count) are described without one. `loadSprite` prefixes the file path.

**`drawSprite(frame, sprite, cellX, cellY, tint)`** draws two pixel rows per cell row, clipping at the framebuffer edges:
- both pixels transparent: the cell is untouched;
- top only: `▀` in the top color over the existing background;
- bottom only: `▄` in the bottom color over the existing background;
- both: `▀` with the top color as foreground and the bottom color as background;
- an odd height treats the last row's bottom pixel as transparent.

**Tints:** `SpriteTint::none()`, `flash()` (opaque pixels white) and `dim(n)` (each channel times (8-n)/8, n clamped to 0-8). They are applied to the sprite color before the cell is written.

**Glyph sprites (`.glyphs`, UTF-8 text):** one cell per character instead of two pixels per cell. `glyphs <w> <h>` (in cells), then `palette` as in `.sprite`, then `chars` with exactly `h` rows written `|...|` (exactly `w` printable ASCII characters between the bars; a space is transparent; the bars keep rows that start with `#` or are all spaces), then `colors` with `h` rows of `w` palette keys (`.` is transparent). A cell must be transparent in both grids or in neither. `parseGlyphs` / `loadGlyphs` / `serializeGlyphs` mirror the `.sprite` functions, with the same line-numbered errors, and non-ASCII characters are reported before row widths. `drawGlyphs(frame, sprite, x, y, tint)` writes each opaque cell's character and tinted foreground, keeps the cell's background, and leaves transparent cells untouched. Tints work as for half-blocks.

**`sprite_bake in.png out.sprite [--max-colors N]`** (default 32, at most 62): decodes a PNG (PNG only), treats alpha below 128 as transparent, keeps colors exactly, and assigns palette keys in order of first appearance. If there are more distinct colors than allowed it fails with "reduce the palette first" and writes nothing. Images larger than 512 px on a side are refused before any pixels are converted. **`--glyphs`** writes a `.glyphs` file instead. The PNG is W x 2H pixels, and each cell covers one column and two rows:
- the cell is transparent if both pixels are;
- otherwise the average Rec. 709 luminance of its opaque pixels, stretched across the image's darkest-to-brightest range and lifted by a square root (so dark-colored areas still get visible glyphs; an image of one brightness uses the absolute scale), picks a character from the ramp `.:-=+*#%@` (dark to bright; the space before it is reserved for transparency);
- the cell's color is the top pixel's, or the bottom's when the top is transparent. No colors are blended, so the palette never grows beyond the image's own.

Unknown flags print the usage and exit 2. The image decoder (`stb_image.h`, pinned commit, SHA-256 checked) is used only by this tool; the game has no image dependency.

**Shipped sprites** (`assets/sprites/`, generated locally with Draw Things, keyed and shrunk by `tools/art/to_sprite_png.py`, then baked with `sprite_bake`; prompts and seeds in `tools/art/README.md`): `hero_fighter`, `hero_hunter`, `hero_rogue` (24x24), `medusa`, `avaritia`, `gula` and `ira` (32x28), `monster` (16x16). Content validation only checks that an enemy's sprite file exists; the sprite test checks that all eight parse at the documented sizes.

**Large art** (`assets/sprites/large/`, optional): `<name>.sprite` (half-blocks) and `<name>.glyphs` for the same names. Heroes are 48x48 px (48x24 cells), `medusa`, `avaritia`, `gula` and `ira` 56x52 px (56x26), and `monster` is 32x32 px (32x16). Both looks share each sprite's cell footprint. `loadLargeSprites(content, dir)` loads whichever of the two files exist for every sprite the screens draw, keyed by the compact file name (`"hero_fighter.sprite"`). A missing file is just absent. A file that exists but doesn't parse stops startup with its path and line, exit 2, before the terminal changes. The ramp for glyphs is stretched to each image's brightness range and lifted by a square root (see `sprite_bake --glyphs` above). Nothing draws the large art yet; the large layouts come in `add-large-art-layout` chunk 4.

**Hero models** (`assets/models/`, window only): `hero_fighter.glb`, `hero_hunter.glb` and `hero_rogue.glb` are rigged, animated glTF characters from the CC0 KayKit Adventurers pack (sources and licence in `assets/models/LICENSES.md`), made with `tools/art/prep_model.py`, which skins their weapons and gear to the bones and turns the cloth to each path's colour. `heroes-window` draws the hero in the Overworld from them as glyphs (see `window.md`). The terminal and every battle still use the sprites.
