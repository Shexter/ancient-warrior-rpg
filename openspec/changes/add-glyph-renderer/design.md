# Design

## Context

See proposal.md for why. What the window builds on:
- **`app::App`** already exposes `update(keys, dt)`, `draw(Framebuffer&)` and `canvasSize(terminal)`, so a second front end can drive the screen stack without the terminal's `engine::Loop`.
- **`engine::FixedStepper`** gives 60 Hz fixed steps. `KeyEvent` already carries `KeyAction` (chunk 2 of `add-free-roam-overworld`), and screens opt in with `wantsKeyActions()`.
- **`OverworldScreen`** owns a `world::World` (hero tile, facing, monsters, bolts, cooldowns), the camera, the HUD, damage numbers, the Nova ring and notices. It draws everything into one `Framebuffer`.
- **Prototype v2** (scratchpad `proto/proto.cpp`, throwaway) proved the look and the layer order at 1280×720. It used 26 px tiles, about 15–25k glyph draws per frame, value-noise moonlight, Gaussian window pools and additive glows.
- **The art pipeline** the user prefers for sprites is Draw Things CLI (FLUX.2 klein 4B), then a chroma key, then `sprite_bake`. Characters in the window use 3D models instead (D8).

## Goals / Non-Goals

**Goals:**
- Game logic stays untouched and headless. The window reads state; it never changes it, apart from what the screens already do through key input.
- The scene is mostly pure code: layer builders turn world state into `GlyphInstance` lists that tests can check, and a thin raylib drawer turns those into draw calls.
- The window is playable end to end early. Chunk 1 already runs every screen, through the framebuffer grid.

**Non-Goals:**
- A browser build, 3D, graphical menus, shaders beyond raylib's built-in blending, and audio.

## Decisions

### D1. Separate executable, optional dependency
`heroes-window` (`src/window/main.cpp`) links `heroes_app`, `heroes_game`, `heroes_engine` and raylib. raylib 5.5 is fetched with `FetchContent` (URL plus SHA256, `BUILD_EXAMPLES OFF`, `SYSTEM`) only when `HEROES_BUILD_WINDOW` is on.
- The terminal `heroes` and `heroes_platform` targets don't change.
- The startup code shared by both binaries (options, data dir, content, world, sprites, save path) moves from `main.cpp` into `app::loadStartup(...)`, which returns everything or the error lines. Both mains call it, so they can't drift apart.

*Alternative:* one binary with `--window`. Rejected, because then the terminal build would always need raylib and GL.

### D2. The window drives `App`, not `engine::Loop`
`window::Run` is a raylib loop: poll input, then `FixedStepper::advance(GetFrameTime())`, then `app.update` per step with that step's keys, then draw.
- The terminal size it reports to `canvasSize` is the window size divided by the cell size. The cell size is the largest whole-pixel scale at which the chosen canvas fits, capped so 80×24 never exceeds the window.
- `engine::Loop` stays the terminal's loop.

*Alternative:* make `Loop` generic over a backend. That's more code for two backends with different draw models.

### D3. Framebuffer grid drawer
`window::drawGrid(const Framebuffer&, origin, cell)` fills each cell's background, then draws its glyph centred.
- Half-block glyphs (`▀`, `▄`, `█` and the shades) are drawn as rectangles, not font glyphs, so they tile without seams.
- The font atlas covers ASCII plus every non-ASCII code point the screens use. A test greps the source for the literals and checks the atlas has each one.

### D4. The Overworld seam: HUD-only drawing and a scene view
`OverworldScreen` gains two methods:
- `drawHud(Framebuffer&, Size viewTiles)`: draws the panels, minimap and hotbar, and leaves every other cell's glyph `U'\0'`, meaning "nothing here". The camera follows the hero over a view of `viewTiles` map tiles, so clicks (read against the camera) map to the scene's tiles.
- `sceneView() const`, which returns a `SceneView` struct (plain data): world def, `World` ref, camera in tiles, the damage numbers and Nova ring (notices stay HUD-side), the hero's and monster's battle art, the hero's name, and the tick counter.

The window checks whether the top screen is an `OverworldScreen` (one `dynamic_cast` in `window::Run`). If it is, it draws the scene from `sceneView()` and then the HUD grid, skipping `\0` cells. Otherwise it draws the grid.

*Alternative:* a virtual "scene" interface on `Screen`. Rejected, because it would have one implementation (ponytail rule: no interface with one implementation).

### D5. Move progress for interpolation
`World` exposes per-creature `from`, `to` and `progress`:
- the hero from `moveTicksLeft_` and `ticksPerTile`;
- monsters gain `from` and `stepTicks` set when they step (chase or wander step time), and an `id`, a serial number given at spawn, so the window can keep each one's animation pose (D8) when others are removed.

This is read-only and doesn't change the simulation. The window also interpolates within a step, by the stepper's leftover fraction (`alpha`), so motion is smooth at any refresh rate.

### D6. Layer builders
`window/scene.*`: `buildScene(const SceneView&, float time, Rect viewPx) -> Scene`, where `Scene` holds vectors per layer.
- **Glyph instances** are `{x, y, size, angle, anchor (centre or base), glyph, rgba}`.
- **Shape instances** are `{rect or ellipse or radial gradient, rgba, blend}`.

Each layer is one function, ported from prototype v2 and tidied:
- `groundLayer`, `waterLayer`, `surfaceLayer` (cobbles, path, rock, cave mouths), `lightLayer` (window pools, under the grass as in v2), `grassLayer`, `treeLayer`, `houseLayer` (houses found from wall blocks once, cached per `WorldDef`), `creatureLayer` (with reflections and wind-up tiles in an under-grass layer), `effectLayer`.
- Items go into nine fixed layers (`Ground, Pools, Under, Grass, Trees, Houses, Creatures, Glow, Text`), each an ordered list, so shapes and glyphs interleave correctly within a layer. Moonlight is a colour shade on every item, not a layer. The vignette is drawn by the drawer in screen space.
- Shapes are drawn with the font atlas's white corner, so the scene batches as one texture.

Randomness is a position hash only (the prototype's `hash`/`rnd`/`noise`), never `Rng`. Buffers are reused between frames (`Scene::clear()` keeps capacity), so there's no per-frame allocation once warm. The drawer submits instances in layer order, switching blend modes per layer.

### D7. Monster and hero bend radius, wind-up and effects
- Grass bends away from every creature within 46 px, not just the hero.
- A wind-up draws a red tile-sized glow under its target and alternates the monster's tint every 4 ticks, matching the terminal behaviour.
- Damage numbers, bolts and the Nova ring come from the same `OverworldScreen` state the terminal draws, so both front ends show the same events.

### D8. Characters as animated 3D models drawn in glyphs
Hand-drawn frames per facing snap between 4 directions and a few poses; the user wants characters that look "almost 3d" and move fluidly. raylib 5.5 already loads glTF with skeletal animation (`LoadModel`, `LoadModelAnimations`, `UpdateModelAnimation`), so no new dependency is needed.
- **Assets:** `assets/models/hero_<pathId>.glb`, from the CC0 KayKit Adventurers pack: Knight for Fighter, Rogue_Hooded with a crossbow for Hunter, Rogue for the rogue path (Pilgrim). Licences go in `assets/models/LICENSES.md`. The user approved a turntable contact sheet (each model at 8 angles, mid-walk, plus idle, attack and hit) before they were committed.
  - The clips are KayKit's own: `Idle`, `Walking_A`, `1H_Melee_Attack_Chop` and `Hit_A` play idle, walk, attack and hit.
  - `tools/art/prep_model.py` makes each file from the pack's GLB. KayKit hangs weapons and gear as unskinned meshes on bone nodes, and raylib only animates skinned meshes, so the tool keeps one loadout and skins each piece to its bone. It also turns the cloth to the path's colour (green for Hunter, purple for Pilgrim; Knight's crimson already matches).
  - Monsters keep their battle sprites for now. No CC0 glTF fox, bat or bull was found, and Quaternius's horse and cow are FBX only. A later `monster_<tier>.glb` is picked up the same way.
- **Rendering:** every creature in view gets a slot in one shared render texture (48×64 px each). Its model is drawn into the slot with a fixed three-quarter top-down orthographic camera and a key light matching the moon, and the texture is read back once per frame.
- **Glyphs:** a pure function `glyphsFromPixels(pixels, w, h, feet, scale)` turns every pixel with alpha into a glyph Item in the Creatures layer. The character comes from a brightness ramp (the colored glyph ramp the user liked), drawn in the pixel's own colour, on the existing dark silhouette at about 2.2 scene units per pixel. Shadows, reflections, name labels and grass bending keep working off the same Items and feet. It is a pure function, so it can be tested without a GPU.
- **Pose:** a pure function `poseOf(previous pose, creature state, dt)` gives `{clip, clipTime, fromClip, fromTime, blend, yaw}`:
  - `yaw` eases toward the movement direction, reaching it in about 0.12 s, taking the short way round.
  - `walk` advances by move progress (D5) times a cycle length per tile, so feet don't slide at any pace. `idle` advances by time.
  - `attack` starts when the hero uses an ability (its cooldown rises) and `hit` when a damage number lands on the hero; each plays once, then it returns to walk or idle. The window reads both from the scene view, so the World needs no new events.
  - A clip change crossfades over 0.15 s. raylib 5.5 doesn't blend animations, so the two poses' bone transforms are lerped (slerp for rotation) before the mesh is skinned.
  - Pose state is kept per creature in the window, keyed by monster identity, and never feeds back into the World.
- **Loading:** models load at window startup. A missing file falls back to the battle sprite; a file that fails to load, or is missing a required clip, exits with code 2 naming it.
- The terminal Overworld keeps its `@`.

### D9. Font
JetBrains Mono (SIL OFL 1.1) Regular in `assets/fonts/` with its licence, loaded at 64 px with bilinear filtering.

### D10. Window close
raylib's exit key is turned off (`SetExitKey(KEY_NULL)`), so Esc stays a game key. raylib 5.5 raises `WindowShouldClose()` for one frame per close-button press and then clears it, so the window can decline.
- With nothing unsaved (`session.dirty` false, the same test the Hub's Quit uses) the window closes at once.
- Otherwise it pushes `app::QuitPromptScreen(behind)` (through the new `App::push`), a tiny screen showing a copy of the last frame with the existing `ConfirmBox`: Yes quits, No or Esc pops.
- A shared helper turned out unnecessary: the only shared rule is that one `dirty` test.

## Risks / Trade-offs

- **[Linux CI needs GL/X11 headers]** → The workflow installs `libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev` on Ubuntu. Window tests that need a display are labelled `window-display` and skipped in CI. Pure layer-builder tests run everywhere.
- **[Frame time with dense grass at 1920×1080]** → Recorded in a benchmark task. If it goes over budget, grass blades per tile scale with zoom, and the static layers (ground, surfaces, houses) are cached into a render texture redrawn only when the camera crosses a tile.
- **[Model look as glyphs]** → The approval gate in D8 uses the real glyph rendering, not plain renders. The window falls back to the battle sprite if a model is missing, so a gap is never a crash; only malformed files fail startup.
- **[GPU readback per frame]** → One small readback for all creatures, about 48×64 px per creature. The benchmark is rerun after chunk 3; if it goes over budget, the pixels-to-glyphs step moves into a fragment shader.
- **[CC0 packs don't match the hero designs]** → Model materials are recoloured to each path's palette at load time. If the user still rejects the look, the fallback is hand-made models, done as a separate change.
- **[Two front ends drift]** → Both share `loadStartup`, the screens and the `World`. The only front-end-specific code is drawing and input.

## Migration Plan

This is additive. The terminal build behaves identically. Users who don't want the window configure with `-DHEROES_BUILD_WINDOW=OFF`. Rolling back means deleting `src/window/`, the option and the assets.

## Open Questions

- Should the window show the large 120×40 layouts at a bigger cell scale, or always 80×24 scaled up? Either fits D2/D3; it's decided at playtest.
- Do battles later get an art pass in the window, like the Overworld? That would be a separate change.
