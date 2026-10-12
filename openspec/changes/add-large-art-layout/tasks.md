# Tasks

## 1. Canvas size chosen per screen

- [x] 1.1 Change `engine::Loop` to ask for the canvas size each frame and reallocate the canvas only when it changes; keep the 80×24 minimum and the resize notice; verify loop tests for 80×24, 120×40 centered in 150×45, and a size change between frames
- [x] 1.2 Add `Screen::canvasSize(terminal, options)` (default 80×24) and route it through `App`; verify every existing screen test passes unchanged and a test screen asking for 120×40 is centered in a 150×45 fake terminal
- [x] 1.3 Add `--layout auto|compact` and `--art glyph|blocks` to `parseOptions` and `Session`; verify option tests for defaults, both values, and invalid values (error + usage, exit 2)
- [x] 1.4 Update SimpleDocs (`terminal.md`, `app.md`); verify they describe the per-screen canvas and the flags
- [x] 1.5 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 2. Glyph sprites

- [x] 2.1 Add `GlyphSprite`, `parseGlyphs`/`loadGlyphs` for the `.glyphs` format (design Decision 4) with line-numbered errors; verify parse tests for a valid file, a short row, an unknown key, a char/color transparency mismatch, and non-ASCII characters
- [x] 2.2 Add `drawGlyphs` with the shared flash and dim tints; verify tests that a transparent cell keeps its existing glyph and background and that tints change only the foreground color
- [x] 2.3 Add `sprite_bake --glyphs` (W×2H PNG → `.glyphs`, ramp ` .:-=+*#%@`, palette cap 62); verify bake tests with a black/white/transparent fixture PNG and the too-many-colors error
- [x] 2.4 Update SimpleDocs `sprites.md` and `tools/art/README.md`; verify the format and bake option are documented
- [x] 2.5 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 3. Large art

- [x] 3.1 Generate the large art with Draw Things: three heroes (48×48), Medusa and Avaritia (56×52), the training monster (32×32). Bake each in both looks into `assets/sprites/large/` and record prompts and seeds in `tools/art/README.md`; verify every file parses, and show the user a preview of all twelve before committing
- [x] 3.2 Load optional large art into the sprite bank (missing = absent; malformed = startup error naming the file, exit 2, before the terminal changes); verify loader tests for present, missing and malformed files, and the existing bad-content CTest still passes
- [x] 3.3 Update SimpleDocs `sprites.md` for the large set; verify the doc lists every shipped large file
- [x] 3.4 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 4. Large layouts for the art screens

- [x] 4.1 Give `BattleView` a layout struct and make the current constants the compact instance; verify a test that compact battle frames are byte-identical to before the change for a fixed event sequence
- [x] 4.2 Add the 120×40 Battle layout (design Decision 6) for both looks; verify tests at 120×40 for sprite placement, log length, the skill overlay, and that resizing to 100×30 mid-battle switches to compact with the battle state unchanged
- [x] 4.3 Add the 120×40 Hub, Create Hero and Training playback layouts; verify tests at 120×40 for each, plus fallback to 80×24 when a needed large sprite is missing and with `--layout compact`
- [x] 4.4 Record full-redraw and diff bytes per frame for a 120×40 battle in `docs/benchmarks.md`; verify the numbers are measured, not estimated
- [x] 4.5 Run the real binary in a 150×45 pseudo-terminal through Hub → Battle with `--art glyph` and `--art blocks`, and shrink it mid-battle; confirm the layouts and fallback by screen capture. Update SimpleDocs and mark world-bible §11 items 5–6 as shipped
- [x] 4.6 Complete chunk quality check against `docs/engineering/code-correctness.md`.
