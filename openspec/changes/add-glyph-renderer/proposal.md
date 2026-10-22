# Proposal

## Why

The user's reference is an ASCII MMO whose glyphs are drawn freely: they're rotated, overlapping, layered and lit, and they move smoothly. A terminal can't draw that, because it's one character per cell with no overlap, rotation or in-between positions. Mockups confirmed it (2026-10-21): grid renders at 200 or 400 columns couldn't match the reference. A throwaway raylib prototype (v2) did, and the user chose it as the direction: "v2 is the direction".

The user also said the prototype "exposes our bad character design, other parts looks good". The hero art is a front-facing battle portrait shrunk into a top-down world, so the hero needs art made for the world view.

## What Changes

- **New `heroes-window` executable**, a raylib window front end for the same game.
  - Game logic, content, saves and the screen stack are untouched.
  - The terminal `heroes` binary stays as it is, and doesn't link raylib.
- **Every screen works in the window from the start.** A screen's 80×24 (or large) framebuffer is drawn as a grid of glyphs at the window's scale. The Hub, towns, battles, training and dialogs play the same as in the terminal.
- **The Overworld becomes a layered glyph scene**, as in prototype v2, with the HUD panels and hotbar drawn over it from the same framebuffer. The layers, bottom to top:
  - ground fills;
  - water with ripples, glints and reflections;
  - cobbles, paths and rocks;
  - grass blades that are rotated, sway, and bend away from creatures;
  - trees as glyph domes with shadows;
  - houses with shingled roofs and lit windows;
  - creatures on dark silhouettes, with shadows and name labels;
  - firefly and Nova glows;
  - light: moonlit patches and warm window pools;
  - vignette.
- **Smooth movement.** The world stays tile-based and deterministic. The window interpolates creatures between tiles over each move, and bolts in flight too.
- **Native key events.** The window reports key press, repeat and release, so held keys work without the terminal fallback. Clicks map to world tiles.
- **Characters as animated 3D models drawn in glyphs.** The user wants the characters to look "almost 3d" and move "so fluidly like elden ring"; hand-drawn frames per facing can't do that.
  - Each hero path is a rigged glTF model with idle, walk, attack and hit clips, from the CC0 KayKit Adventurers pack, approved by the user before they're committed. Field monsters keep their battle sprites until CC0 models are found.
  - Each frame the window renders the models off screen and turns their pixels into coloured glyphs, so they face any direction, turn smoothly and blend between animations.
- **A bundled open-licence monospace font** in `assets/fonts/`, so the build doesn't depend on a system font.

## Capabilities

### New Capabilities
- `window-rendering`: the window front end. It covers its options, how framebuffer screens are drawn, the Overworld scene and its layers, smooth movement, input, the world-scale sprites, performance, and staying independent of the game's random stream.

### Modified Capabilities
- None. The `terminal-rendering` requirements are unchanged, and the terminal build behaves exactly as before. The Overworld's behaviour (its `overworld` and `field-combat` capabilities, from `add-free-roam-overworld`) is unchanged; the window only draws it differently.

## Impact

- **Build:**
  - raylib 5.5 is fetched with `FetchContent`, pinned by hash, like the other dependencies.
  - A new `HEROES_BUILD_WINDOW` option builds `heroes-window` and its tests (on by default; off skips raylib entirely).
  - Linux CI needs the X11/GL development packages, and the CI workflow gains an install step for them.
- **New code:**
  - `src/window/` holds the raylib front end: the main loop, input, the framebuffer-grid drawer and the Overworld scene.
  - The scene is split into pure "layer builders" that turn world state into glyph instances, which can be tested without a window, and a thin raylib drawer.
- **Small changes to existing code:**
  - `World` exposes each creature's move progress, so the window can interpolate.
  - `OverworldScreen` can draw its HUD alone, with the map cells marked transparent, for the window to put over the scene.
- **Assets:** new CC0 character models with their licences under `assets/models/`, and a font with its licence under `assets/fonts/`.
- **Not affected:** game rules, the save format, data files, balance, the battle goldens, or the terminal binary's behaviour and dependencies.
- **Depends on:** `add-free-roam-overworld` through chunk 6 (its chunk 6 GCC check still has to pass first). Its chunk 7, the full map, can land before or after this change.
- **Non-goals:**
  - a browser build (raylib supports Emscripten; it's a later change);
  - 3D rendering;
  - turning menu screens into graphical layouts (they stay glyph grids);
  - multiplayer.
