# Window front end (`src/window/`, `heroes-window`)

`heroes-window` is a raylib front end for the same game. It's built when `HEROES_BUILD_WINDOW` is on, which is the default. It plays through the same `App`, screens, content, world and saves as `heroes`. The terminal `heroes` never links raylib, and with the option off nothing from raylib is downloaded.

## Build

- raylib 5.5 is fetched by `FetchContent` in `cmake/Dependencies.cmake`, pinned by SHA256, with examples and games off and as a static library. It is only fetched when the option is on.
- The font is JetBrains Mono Regular, `assets/fonts/JetBrainsMono-Regular.ttf`, under the SIL OFL 1.1 (`assets/fonts/JetBrainsMono-OFL.txt`).
- Targets:
  - `heroes_window_lib` (input, grid, run);
  - `heroes-window`;
  - `heroes_window_tests`: pure tests, no display;
  - `heroes_window_display_tests`: opens a hidden window, labelled `window-display`. CI runs `ctest -LE window-display`, and Linux CI installs the X11 and GL development packages.

## Startup

- `heroes-window [--seed N] [--data-dir PATH] [--art glyph|blocks]` goes through `app::loadStartup` (see `app.md`) exactly as `heroes` does.
- `--layout` is rejected as an unknown option, with exit code 2.
- After that it checks for the font at `<data-dir>/../assets/fonts/JetBrainsMono-Regular.ttf`. A missing font prints `heroes-window: error: font not found: …` and exits with code 2. Every one of these errors happens before a window opens.

## Loop (`window::run`)

- **Window:** opened at 1280×720, resizable, with vsync, and at least 800×450. raylib's exit key is off, so Esc is a game key. raylib logs errors only.
- **Font:** loaded at 48 px over `atlasCodepoints()` (printable ASCII plus the non-ASCII code points used anywhere in `src/app` and `src/engine`), with bilinear filtering.
- **App:** driven directly, with a do-nothing `LoopIo` and an unused terminal `Renderer`. `engine::Loop` isn't used.
- **Each frame:**
  1. **Keys are read** (`pollKeys`) and a left click becomes `Key::Click` at the canvas cell under the pointer (margin clicks are dropped). These are added to the pending keys.
  2. **The close button:** with nothing unsaved the window closes. Otherwise it pushes `QuitPromptScreen` with a copy of the last frame, unless one is already on top.
  3. **Steps run:** `FixedStepper` runs 0–5 steps of 1/60 s. The pending keys go to the first step, and are kept if no step ran. A step that ends the app closes the window.
  4. **The frame is drawn:**
     - `terminalFor(window)` gives the window's size in base cells of 9×18 px.
     - `acceptedCanvas(App::canvasSize(that), that)` keeps the phone canvas or anything from 80×24 up that fits, and otherwise uses 80×24.
     - `layoutFor(canvas, window)` picks the largest whole-pixel cell (width w, height 2w) that fits, and centres it.
     - The screen draws into the framebuffer. The window clears to cell (0, 0)'s background, then calls `drawGrid`.
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
- With `skipEmpty`, cells whose glyph is `U'\0'` are left untouched. This is for the Overworld scene, which isn't built yet (add-glyph-renderer chunk 2).
- The display test checks that every cell's background lands at its layout position and that glyphs show in their foreground colours. Set `HEROES_SNAPSHOT_DIR` to make it write `window_title.png` there for a look by eye.

Until chunk 2, the Overworld draws in the window as its terminal grid.
