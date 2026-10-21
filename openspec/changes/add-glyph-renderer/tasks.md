# Tasks

## 1. Window front end with every screen

- [ ] 1.1 Move the shared startup (options, data dir, content and mods, world, sprites, save path) from `src/app/main.cpp` into `app::loadStartup`, returning everything or the error lines (D1). Verify that `heroes` behaves as before: the existing CLI tests (`heroes_reports_bad_content`, `heroes_reports_bad_world`, `heroes_rejects_bad_layout`) pass, and a new unit test covers `loadStartup`'s error lines.
- [ ] 1.2 Add `HEROES_BUILD_WINDOW` (default ON) and raylib 5.5 through `FetchContent` with a pinned hash (D1). Bundle JetBrains Mono and its OFL licence (D9). Verify that configuring with the option OFF builds and passes every test without downloading raylib, and that with it ON `heroes-window` links.
- [ ] 1.3 Add `window::Run`: a raylib loop driving `App` with `FixedStepper`, the cell-size and canvas fit (D2),, and input mapped to `KeyEvent` with press, repeat and release. Clicks go to canvas cells, and Esc is not an exit key. Verify with unit tests for the key mapping table and the cell-fit function (80×24 in 1280×720, a resize, the minimum).
- [ ] 1.4 Add `window::drawGrid` (D3): backgrounds, centred glyphs, half-block glyphs as rectangles, and a font atlas covering every code point the screens use. Verify with a test that every non-ASCII literal in `src/app` is in the atlas set, and a hidden-window smoke test (label `window-display`) that renders the Title screen and checks a pixel of the banner.
- [ ] 1.5 Add `app::QuitPromptScreen` and the shared quit helper (D10). The window's close button uses them. Verify with screen tests that Close with unsaved changes asks and that No keeps playing, and that the Hub's quit tests still pass.
- [ ] 1.6 Add the window CLI: `heroes-window` rejects `--layout` and reports startup errors with exit code 2 before opening a window. Verify with CTest cases mirroring `heroes_reports_bad_world` and `heroes_rejects_bad_layout`.
- [ ] 1.7 Install the Linux GL/X11 packages in CI and skip `window-display` tests there. Verify all compilers locally (clang debug, asan and ci; GCC 14 in Docker with the packages; a mingw syntax check of the window sources).
- [ ] 1.8 Add SimpleDocs `window.md` (the executable, build option, loop, input, grid drawing) and update `app.md` for `loadStartup` and the quit prompt. Verify that they match the code.
- [ ] 1.9 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 2. The Overworld scene

- [ ] 2.1 Add the Overworld seam (D4): `OverworldScreen::drawHud(frame, transparentMap)` and `sceneView()`. The window draws the scene under the HUD grid, skipping `\0` cells. Verify with screen tests that the transparent HUD frame leaves exactly the map cells as `\0`, and that the terminal golden frames are unchanged.
- [ ] 2.2 Add the scene types and the drawer (`GlyphInstance`, shapes, a blend per layer), with buffers reused between frames (D6). Verify with a test that building the scene twice reuses capacity (no growth on the second build) and that the drawer submits layers in order (a fake drawer records the calls).
- [ ] 2.3 Add `groundLayer`, `surfaceLayer` (cobbles, path, rock with the cave mouth), `treeLayer` and `houseLayer`, with houses found from wall blocks and cached, ported from prototype v2. Verify with layer tests: the Stable look scenario at fixed time, every tree tile producing a dome, four houses found on the starter map with their windows, and nothing drawn outside the view rectangle.
- [ ] 2.4 Add `grassLayer`, with blades anchored at their base, swaying, and bending away from every creature within 46 px; and `waterLayer`, with ripples, glints and reflections mirrored about a creature's feet and clipped to water tiles. Verify with tests that a blade beside a creature leans away from it, that a reflection appears only on water tiles, and that blade counts per tile type are as specified.
- [ ] 2.5 Add `creatureLayer`, `effectLayer` and `lightLayer`. Creatures get silhouettes, shadows and name labels (using the battle sprite until chunk 3). Effects cover wind-up highlights and flashes, bolts, the Nova ring, damage numbers and fireflies; light covers moon patches, window pools and the vignette (D7). Verify with tests for Wind-up stays readable and the Drawing leaves the game untouched scenario (100 builds, state equal).
- [ ] 2.6 Measure the frame time at 1280×720 and 1920×1080 over 600 frames in a release build, and record it in `docs/benchmarks.md`. If the 1280×720 average is over 16.6 ms, add the static-layer render-texture cache from the risks section and measure again. Verify that the numbers are recorded and meet the Frame budget scenario.
- [ ] 2.7 Update SimpleDocs `window.md` with the scene, its layers and the seam. Verify that it matches the code.
- [ ] 2.8 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 3. Smooth movement and world-scale characters

- [ ] 3.1 Expose move progress from `World` (D5): the hero's and each monster's `from`, `to` and `progress`, and bolts' progress. The window interpolates by those plus the stepper's `alpha`, and the camera follows the drawn position. Verify with World tests for the Halfway through a step scenario (hero and a monster), with the existing World replay tests unchanged, and with a scene test that the hero is drawn halfway.
- [ ] 3.2 Generate the world-scale hero sprites (3 paths × 4 facings × 3 frames) and monster sprites (4 tiers) with Draw Things, bake them, record prompts and seeds in `assets/sprites/world/PROMPTS.md`, and show the user a contact sheet. Commit only once the user has approved it (D8). Verify that the approval is recorded in `notes.md`.
- [ ] 3.3 Load and validate world sprites at startup (a missing set falls back to the battle sprite; a malformed file exits with code 2). Select frames by facing and walk phase. Verify with tests for the Facing follows movement scenario and a malformed-sprite startup failure.
- [ ] 3.4 Playtest manually in the window (walk, every landmark, a field fight, a mission and back, close with unsaved changes), and record the findings in `notes.md`. Verify that the notes are written.
- [ ] 3.5 Update SimpleDocs `window.md` and `sprites.md`. Verify that they match the code.
- [ ] 3.6 Complete chunk quality check against `docs/engineering/code-correctness.md`.
