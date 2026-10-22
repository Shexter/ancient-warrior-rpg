# Window front end (`src/window/`, `heroes-window`)

`heroes-window` is a raylib front end for the same game. It's built when `HEROES_BUILD_WINDOW` is on, which is the default. It plays through the same `App`, screens, content, world and saves as `heroes`. The terminal `heroes` never links raylib, and with the option off nothing from raylib is downloaded.

## Build

- raylib 5.5 is fetched by `FetchContent` in `cmake/Dependencies.cmake`, pinned by SHA256, with examples and games off and as a static library. It is only fetched when the option is on.
- The font is JetBrains Mono Regular, `assets/fonts/JetBrainsMono-Regular.ttf`, under the SIL OFL 1.1 (`assets/fonts/JetBrainsMono-OFL.txt`).
- Targets:
  - `heroes_window_lib` (input, grid, scene, models, run);
  - `heroes-window`;
  - `heroes_window_tests`: pure tests, no display;
  - `heroes_window_display_tests`: opens a hidden window, labelled `window-display`. CI runs `ctest -LE window-display`, and Linux CI installs the X11 and GL development packages.

## Startup

- `heroes-window [--seed N] [--data-dir PATH] [--art glyph|blocks]` goes through `app::loadStartup` (see `app.md`) exactly as `heroes` does.
- `--layout` is rejected as an unknown option, with exit code 2.
- After that it checks for the font at `<data-dir>/../assets/fonts/JetBrainsMono-Regular.ttf`. A missing font prints `heroes-window: error: font not found: …` and exits with code 2.
- Then `loadModelFiles` reads the animations of `<data-dir>/../assets/models/hero_<pathId>.glb` for each path. A missing file is skipped (that hero keeps its battle sprite). A file that doesn't start with the binary glTF magic `glTF` is refused before raylib sees it (raylib leaks a file it can't parse). A file that doesn't load prints `heroes-window: error: <file>: not a loadable animated model`, and one without one of the clips `Idle`, `Walking_A`, `1H_Melee_Attack_Chop` and `Hit_A` prints `<file>: missing clip '<name>'`. Both exit with code 2.
- Every one of these errors happens before a window opens.

## Loop (`window::run`)

- **Window:** opened at 1280×720, resizable, with vsync, and at least 800×450. raylib's exit key is off, so Esc is a game key. raylib logs errors only.
- **Font:** loaded at 48 px over `atlasCodepoints()` (printable ASCII plus the non-ASCII code points used anywhere in `src/app` and `src/engine`), with bilinear filtering.
- **Models:** a `ModelPainter` loads each model's meshes once the window is open. A model whose meshes don't load, or aren't all skinned to the armature, prints its file and exits with code 2.
- **App:** driven directly, with a do-nothing `LoopIo` and an unused terminal `Renderer`. `engine::Loop` isn't used.
- **Each frame:**
  1. **Keys are read** (`pollKeys`) and a left click becomes `Key::Click` at the canvas cell under the pointer (margin clicks are dropped). If the last frame drew the Overworld scene, a click is instead the map tile under the pointer as last drawn (`sceneTileAt`, through the drawn camera), counted from the screen's camera tile (which is how the Overworld reads clicks). These are added to the pending keys.
  2. **The close button:** with nothing unsaved the window closes. Otherwise it pushes `QuitPromptScreen` with a copy of the last frame, unless one is already on top.
  3. **Steps run:** `FixedStepper` runs 0–5 steps of 1/60 s. The pending keys go to the first step, and are kept if no step ran. A step that ends the app closes the window.
  4. **The frame is drawn:**
     - `terminalFor(window)` gives the window's size in base cells of 9×18 px.
     - `acceptedCanvas(App::canvasSize(that), that)` keeps the phone canvas or anything from 80×24 up that fits, and otherwise uses 80×24.
     - `layoutFor(canvas, window)` picks the largest whole-pixel cell (width w, height 2w) that fits, and centres it.
     - The screen draws into the framebuffer. The window clears to cell (0, 0)'s background, then calls `drawGrid`.
     - The Overworld is the exception: the window draws its scene with the HUD over it (below). Before the frame starts it moves the hero's pose on and paints the hero's model (below).
- **Exit code:** 0, or 1 on an exception (printed as `heroes-window: error: …`).

## Input (`window/input.*`)

- **Keys with press, repeat and release** (`IsKeyPressed`, `IsKeyPressedRepeat`, `IsKeyReleased`):
  - the arrows, Enter (and keypad Enter), Esc, Backspace and Tab;
  - letters (upper-cased with Shift);
  - digits (only without Shift);
  - Space.
- **Every other printable character** comes from raylib's character queue as a press. Characters those keys already produce are skipped there, so nothing arrives twice. Shifted digits (`!`, `@`, …) come from the queue.

## Grid drawing (`window/grid.*`)

- For each cell, `drawGrid` fills the background and then draws the glyph centred, at 92% of the cell height.
- `▀ ▄ █` and the box-drawing lines `─ │ ┌ ┐ └ ┘` are drawn as rectangles, so half-block sprites and panels tile without seams. Spaces draw only their background.
- With `skipEmpty`, cells whose glyph is `U'\0'` are left untouched, so the Overworld scene shows through.
- The display test checks that every cell's background lands at its layout position and that glyphs show in their foreground colours. Set `HEROES_SNAPSHOT_DIR` to make it write `window_title.png` there for a look by eye.

## The Overworld scene (`window/scene.*`, `window/scene_draw.cpp`)

When the top screen is an `OverworldScreen`, the window draws the map as freely placed glyphs and shapes instead of a grid. `drawOverworld` does it each frame.

- **The seam:** `OverworldScreen` gives the window two things.
  - `drawHud(frame, viewTiles)` draws only the status panel, goal line, minimap and hotbar, exactly as the terminal does. Every other cell gets glyph `U'\0'`. It also moves the camera to keep the hero in a view of `viewTiles` map tiles.
  - `sceneView()` is plain data: the map, the `World`, the camera tile, the hero's and the monster's battle art (the large block sprite when loaded, else the 80×24 one), the hero's name, the step counter, the damage numbers and the Nova ring.
  - The terminal's `draw` is unchanged: the map, then the same HUD.
- **Units and zoom:** a map tile is `kTilePx` = 26 scene units. The window shows 720 units of height at any size (`sceneZoom` = height / 720), so a 1280×720 window shows about 49×28 tiles.
- **Camera:** the view's top-left is the scene's drawn camera (`Scene::camera`), which eases toward the screen's camera tile each frame (`1 - e^(-12·dt)` of the way, so most of the way in 0.2 s) and so stays inside the map. It starts on the screen's camera when the Overworld is first drawn, and again after another screen was shown.
- **Smooth movement:** the hero, monsters and bolts are drawn between tiles from the World's `Motion` (from, to, progress, progress per tick), plus `alpha`, the stepper's leftover fraction of a step (`FixedStepper::alpha`), capped at the destination. A bolt is drawn heading for the tile it reaches next. Grass bends away from the drawn feet.
- **Hero model** (`window/models.*`): when the hero's path has a model, the hero is drawn from it instead of its battle sprite. Monsters have no models yet and keep their sprites.
  - **Pose:** `poseOf(previous, doing, dt)` is pure. `doing` is how far the drawn hero moved since the last frame (in tiles), whether an ability's cooldown went up (an attack started), and whether a damage number on the hero appeared. The yaw eases toward the way it moves (`1 - e^(-dt/0.04)` of the turn per frame, most of it in 0.12 s, the short way round) and stays put when it stops. Walk advances 0.45 clip seconds per tile walked, so its pace follows the move; idle advances by time. Attack and hit play once (1.067 s and 0.667 s), then it walks or idles. A clip change starts the new clip at 0 and fades it in over the old one, frozen where it was, in 0.15 s. The window keeps the pose between frames and resets the last position on entering the Overworld; it never touches the game.
  - **Painting:** `ModelPainter::paint(pathId, pose)` mixes the two clips' frames bone by bone (lerp for position and scale, slerp for rotation; raylib 5.5 can't blend), skins that, and draws the model into a 48×64 render texture with a fixed orthographic camera pitched 40°, lit by a small built-in shader (key light from the upper left, ambient and a cool rim). The pixels are read back once per frame.
  - **Glyphs:** `glyphsFromPixels(out, pixels, feet, scale)` is pure. Every pixel with alpha 40 or more becomes a dark silhouette cell, then a glyph from ` .:-=+*#%@` (from `-` up, by brightness) in its colour, lifted so its brightest channel reaches at least 150 (and by at least 1.2). They're `kModelScale` = 2.2 scene units a pixel, centred on the feet, with the lowest drawn row standing on them. It returns the drawn height, which places the name label. The reflection is the same glyphs mirrored about the feet, where they fall on water.
- **Building:** `buildScene(scene, view, time, viewRect, alpha, hero)` clears and refills the scene's per-layer item lists, keeping their capacity, so a warm scene allocates nothing.
  - An item is a glyph (centred, or a blade standing on its foot, each with a size and a rotation), a rectangle, an ellipse, or a glow (a colour fading to clear).
  - Only tiles in view plus a 2–3 tile margin are visited, and items wholly outside that margin are dropped.
  - Every look comes from a hash of the map position and from `time`. Nothing reads or changes the game's random stream or state.
- **Layers, bottom to top** (`Layer`):
  1. **Ground:** dark fills under grass and trees, and wall bases. Water has drifting ripple rows (`- ~ =`) and the odd moon glint. Cobbles (floor) use `oOoc.Q` stones, rotated, with some missing; path and sand use `.:,;` dust; rock uses boulder glyphs shaded by noise. A cave mouth is a black ellipse where a path tile has rock above it.
  2. **Pools** (additive): warm light on the ground below each house window.
  3. **Under:** a red highlight and glow on each tile a monster is winding up to hit, and the creatures' reflections, mirrored about their feet and drawn only where they fall on water.
  4. **Grass:** 14 blades per grass tile and 26 per tall-grass tile, each with its own position, lean, size, shade and opacity. They sway over time, and lean away from any creature (the hero or a monster) whose feet are within 46 units.
  5. **Trees:** a shadow ellipse and a dome of 100 glyphs lit from the top-left.
  6. **Houses:** one per block of wall tiles, found once per map and cached in the scene. Each gets a shadow, shingle rows (lit back slope, dark front slope, a ridge), a plank wall, a door and a glowing window.
  7. **Creatures:** each landmark's mark (its initial on a gold disc, or `!` on red for a lair); then each monster and the hero, each with a ground shadow, drawn as glyph cells on a dark silhouette from their battle sprite, or the hero from its model (below). The hero's sprite faces the way it last moved (mirrored when facing left), with its name above. A monster winding up flashes toward gold every 4 steps, as in the terminal.
  8. **Glow** (additive): fireflies over about 7% of grass and water tiles, bolts, and the Nova ring.
  9. **Text:** damage numbers rising over the hit tile, red on the hero and gold on monsters.
  - Colours throughout are shaded by moonlit noise patches and warmed near windows.
- **Drawing:** `submit(scene, out)` walks the layers in order. `drawScene` turns items into raylib calls under a `Camera2D` (alpha or additive blending per layer), then draws a vignette over the window edges.
  - Glyphs are drawn straight from the font atlas with `DrawTexturePro`. Rectangles, ellipses and glows use the atlas's white corner, so everything shares one texture and batches together.
  - The HUD grid is drawn last with `skipEmpty`.
- **Tests:** `window_scene_test.cpp` checks buffer reuse, layer order, a stable look when walking away and back, tree domes, the four starter houses with windows and light pools, view culling, blade counts and bending, reflections only on water, a readable wind-up, and that 100 builds (with and without a hero model) leave the game unchanged; also `glyphsFromPixels` on a fixed grid and `poseOf` (facing, short turns, walk pace, the crossfade, attack then idle). The display tests render the scene under the HUD and the Fighter's model in it (with `HEROES_SNAPSHOT_DIR`, they write `window_overworld.png` and `window_hero.png`). The CLI test `heroes_window_reports_bad_model` checks exit 2 for a model file that isn't one. With `HEROES_BENCH` it times 600 frames (`docs/benchmarks.md`).
- **Not yet:** monsters have no models and are drawn from their battle sprite.
