# Design

## Context

See proposal.md for why. Today `engine::Loop` owns one fixed 80×24 `canvas_`, calls the draw function on it, and centers it in a terminal-sized `screen_` (below 80×24 it draws the resize notice instead). `App` draws only the top `Screen`, and every screen assumes 80×24 coordinates. Sprites are half-block only (`engine::Sprite`, `.sprite`), loaded at startup into `SpriteBank` by file name, and every missing file is a startup error.

## Goals / Non-Goals

**Goals:**
- Let a screen choose a bigger canvas when the terminal allows it, without touching screens that don't opt in.
- Add the glyph format alongside half-blocks, sharing tints and the drawing contract.
- Keep 80×24 behavior byte-identical when the terminal is small or `--layout compact` is set.

**Non-Goals:**
- No large versions of menus, town or result screens (user decision, 2026-10-12).
- No walkable world view, tile map or water reflection (world-bible §11 items 1–4 stay ideas).
- No in-game options screen: the look is chosen by command-line flag.
- No persistence of the choice in the save file (saves hold game state only).

## Decisions

**1. The canvas size is chosen per frame by the top screen.**
- `Screen` gets `virtual Size canvasSize(Size terminal, const Options&) const`, which returns 80×24 by default.
- `Loop`'s draw callback becomes a pair: one call asks for the size, the other draws.
- The loop reallocates the canvas only when the size changes (as it already does for `screen_`), then centers it.

Alternative: a global "large mode" flag that all screens read. Rejected: it makes every screen handle both sizes. With the per-screen choice, the ones that don't care need no changes.

**2. The large threshold is one rule.**
- A screen returns 120×40 only if the terminal is at least 120×40, `--layout` is `auto`, and its large sprites exist in the chosen look.
- Otherwise it returns 80×24. The 80×24 minimum and the resize notice are unchanged.
- Because the decision is made each frame, a live resize switches layouts. A screen's state (battle, typewriter, menus) never depends on the layout, so switching mid-battle is safe.

**3. Each layout is a separate draw path over the same state.**
- The art screens get `drawLarge(frame)` next to their current draw.
- `BattleView` takes a layout struct (sprite positions, rows for stats and log, log length) instead of its current constants. The 80×24 values become the compact instance, so the compact output stays byte-identical. A test compares frames before and after the change.

Alternative: scale coordinates by 1.5. Rejected: text rows don't scale, and fractional positions blur alignment.

**4. Glyph format: `.glyphs` text, in the same family as `.sprite`.**
```
glyphs <w> <h>
palette
<key> #rrggbb
chars
<h rows of |w printable ASCII|; space = transparent>
colors
<h rows of w palette keys; '.' = transparent>
```
- A cell is transparent only when both grids say so. A mismatch is a parse error, reported with its line. (Changed in chunk 2: char rows are wrapped in `|...|`, because a row may start with `#`, the comment marker, or be all spaces, which the parser skips as blank. Editors also strip trailing spaces.)
- `GlyphSprite` and `drawGlyphs(frame, sprite, x, y, tint)` live beside `Sprite` and `drawSprite`, and reuse the tint function.
- `.sprite` stays exactly as it is.

Alternative: extend `.sprite` with a glyph layer. Rejected: it would change a format that has shipped files and tests.

**5. Ramp baking.**
- `sprite_bake in.png out.glyphs --glyphs` reads a PNG that is W × 2H pixels, and each cell covers one pixel column and two pixel rows.
- The luminance (Rec. 709) of each cell's opaque pixels is averaged, stretched across the image's own darkest-to-brightest range and lifted by a square root (changed in chunk 3: the absolute scale left dark-colored figures as faint `.`/`:` in the preview), and picks a character from the ramp ` .:-=+*#%@`, with space reserved for transparent. The cell color is the top pixel's, or the bottom's if the top is transparent. (Changed in chunk 2 from the averaged color: averaging blends quantized colors into new ones and would break the palette cap. The cap is 62 colors with the same "reduce the palette first" error.)
- The same PNG bakes both looks, so the half-block and glyph versions of a character always have the same cell footprint.

**6. Large sprite sizes and placement at 120×40** (in cells; half-block heights are pixels / 2):

| sprite | pixels | cells |
|---|---|---|
| heroes | 48×48 | 48×24 |
| medusa, avaritia | 56×52 | 56×26 |
| monster | 32×32 | 32×16 |

- **Battle layout:**
  - row 0 is the title and turn;
  - the arena is rows 2–27: hero at (6, 3), enemy at (60, 2);
  - stats are rows 29–30;
  - the log is rows 32–37 (6 lines);
  - row 38 is the actions, row 39 the hint, and the skill overlay is at rows 26–33.
- **Hub:** the status panel is widened to 120 columns, the hero is at (4, 7) and the menu at (60, 12).
- **Create Hero:** the preview box sits at cols 40–118, with the sprite inside it at (66, 8).
- **Training playback:** the hero is at (6, 4) and the monster at (80, 8).

Exact rows are settled in implementation and recorded in SimpleDocs. They must not cut into the sprite footprints above.

**7. The large art is optional and loaded separately.**
- `assets/sprites/large/<name>.sprite` and `<name>.glyphs` are loaded if they exist, into a second bank keyed by `(name, look)`.
- A malformed file is a startup error like today's. A missing one only disables the large layout for screens that need it.

Alternative: require large art for every sprite. Rejected: it would block adding an enemy until its big art exists.

**8. Flags.**
- `--layout auto|compact` and `--art glyph|blocks` are parsed in `parseOptions` with the existing error and usage path (exit 2).
- They live in `Options` and are copied into `Session`.

## Risks / Trade-offs

- [Glyph art reads as noise at 48 columns] → Planning used the colored ramp the user approved. If a large glyph sprite reads poorly, the user previews every asset before commit (task 3.x) and `--art blocks` remains.
- [Live layout switching mid-animation] → Layout is draw-only and animations run on screen state, so a switch only moves where things are drawn. A test resizes mid-battle and checks the battle state is unchanged.
- [Large frames cost more bytes per update] → Diff rendering still applies. Record full-redraw and diff bytes at 120×40 in `docs/benchmarks.md`, as chunk 8 did for 80×24.
- [256-color terminals] → Glyph colors go through the existing color fallback, so there is no new path.

## Migration Plan

None: no data, save or format changes for existing files. `--layout compact` reproduces today's behavior exactly.
