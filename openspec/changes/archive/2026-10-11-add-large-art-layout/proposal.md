# Proposal

## Why

The art test (world-bible §11, 2026-10-11) showed that the looks the user liked best, 48×48 half-block sprites and colored glyph-ramp figures, don't fit the 80×24 canvas. Core shipped 24×24 sprites and kept 80×24. Now that core is done, a bigger terminal should show the bigger art.

## What Changes

- **Large layout.** When the terminal is at least 120×40, the screens that show art (Battle, Hub, Create Hero, Training playback) use a 120×40 layout with larger sprites. Every other screen keeps its 80×24 design, centered as today. Below 120×40 everything is exactly as now (80×24 stays the minimum and the fallback).
- **Glyph-ramp sprites.** A second sprite format, one character plus one color per cell, where brightness picks the character from a ramp (`.:-=+*#%@`-style). `sprite_bake` gains an option to produce it from a PNG.
- **Large art.** 48×48 heroes, larger enemies and monster, each in both looks (half-block and glyph), made with the existing Draw Things pipeline.
- **Choosing the look.** In the large layout the glyph look is the default, and `--art blocks` switches to half-blocks. `--layout compact` forces 80×24 everywhere, and `--layout auto` (the default) picks by terminal size and follows live resizes.
- Large art is optional per sprite: if a large version is missing, that screen falls back to its 80×24 layout. A malformed large sprite is still a startup error.

## Capabilities

### New Capabilities
- (none)

### Modified Capabilities
- `terminal-rendering`: adds the large layout and its size rule, the glyph sprite format and drawing, and the `--layout` / `--art` options.

## Impact

- **Engine:** `engine/loop` (canvas size per frame instead of a fixed 80×24), `engine/sprite` (glyph sprites and their tints).
- **App:** `Screen` gains an optional preferred canvas size. `BattleView` and the four art screens get a large layout. `SpriteBank` loads optional large art. `main` parses `--layout` and `--art`.
- **Tools:** `sprite_bake --glyphs`, `tools/art/` notes and prompts.
- **Assets:** `assets/sprites/large/` (`.sprite` and `.glyphs` per hero, enemy and monster).
- **Tests:** loop canvas sizing, glyph parse/draw/tint, bake, large-layout screen tests at 120×40, and fallback when large art is missing.
- **Depends on:** `reskin-act-one` chunk 3 (the Avaritia enemy needs large art too). Apply this change after it.
- No change to saves, game logic, content data or golden vectors.
