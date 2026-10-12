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

**`sprite_bake in.png out.sprite [--max-colors N]`** (default 32, at most 62): decodes a PNG (PNG only), treats alpha below 128 as transparent, keeps colors exactly, and assigns palette keys in order of first appearance. If there are more distinct colors than allowed it fails with "reduce the palette first" and writes nothing. Images larger than 512 px on a side are refused before any pixels are converted. The image decoder (`stb_image.h`, pinned commit, SHA-256 checked) is used only by this tool; the game has no image dependency.

**Shipped sprites** (`assets/sprites/`, generated locally with Draw Things, keyed and shrunk by `tools/art/to_sprite_png.py`, then baked with `sprite_bake`; prompts and seeds in `tools/art/README.md`): `hero_fighter`, `hero_hunter`, `hero_rogue` (24x24), `medusa` and `avaritia` (32x28), `monster` (16x16). Content validation only checks that an enemy's sprite file exists; the sprite test checks that all six parse at the documented sizes.
