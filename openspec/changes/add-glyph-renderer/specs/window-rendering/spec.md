# Spec Delta

## Purpose

A graphical window front end for the game, drawing glyphs freely, layered and lit, so the Overworld can look like an ASCII-styled world instead of a terminal grid. It sits on the same game logic as the terminal build and changes nothing it plays by.

## ADDED Requirements

### Requirement: Window executable
The project SHALL build a `heroes-window` executable when the `HEROES_BUILD_WINDOW` option is on (the default). It SHALL take the same command-line options as `heroes`, except `--layout`, which it SHALL reject like any unknown option. It SHALL use the same data directory, mods, saves and save location as `heroes`, so a game saved in one continues in the other. Startup errors (bad content, a bad world, bad sprites, a missing font) SHALL be printed and SHALL exit with code 2 before a window opens. With the option off, the project SHALL build and test without fetching or linking raylib. The `heroes` executable SHALL never link raylib.

#### Scenario: Shared save
- **WHEN** a game is saved in `heroes-window` and `heroes` is started afterwards
- **THEN** Continue loads that game with every persisted field unchanged

#### Scenario: Terminal build stays light
- **WHEN** the project is configured with `HEROES_BUILD_WINDOW=OFF`
- **THEN** it builds and every non-window test passes, and nothing from raylib is downloaded

#### Scenario: Bad world in the window build
- **WHEN** `heroes-window` starts with a world map that has an unknown tile character
- **THEN** it prints the same error as `heroes` and exits with code 2 without opening a window

### Requirement: Framebuffer screens in the window
Every screen except the Overworld SHALL be drawn by rendering the screen's framebuffer as a grid of glyphs. Each cell's character, foreground and background SHALL be drawn at a cell size scaled to fit the window, with the canvas centred and the margin in the screen's background colour. A screen SHALL get the canvas it asks for as if the terminal were the window's size in cells. Half-block and glyph sprites SHALL look as they do in a truecolor terminal. Screens SHALL behave exactly as in the terminal build.

#### Scenario: Battle in the window
- **WHEN** a mission battle is played in `heroes-window` with the same seed and keys as in `heroes`
- **THEN** the battle's events and result are identical in both

#### Scenario: Window resized
- **WHEN** the window is resized during the Hub
- **THEN** the Hub is redrawn centred at the new cell scale, and the game state is unchanged

### Requirement: Overworld scene
The window SHALL draw the Overworld as a layered scene of freely placed glyphs and shapes instead of a cell grid. The layers, bottom to top, SHALL be:
- ground;
- water, with drifting ripples and the reflections of creatures standing next to it;
- cobbles, paths and rocks;
- warm light pooling on the ground around windows;
- the target tiles of monster wind-ups, and creature reflections;
- grass blades, each anchored at its base with its own rotation, size and colour, swaying over time and bending away from creatures within a short radius;
- trees, with shadows;
- houses, drawn from the map's wall blocks, with roofs and lit windows;
- creatures, each on a dark silhouette with a ground shadow, with the hero's name above it;
- glows, for fireflies, Nova and bolts;
- damage numbers;
- a vignette.

Every layer's colours SHALL be shaded by moonlit patches and warmed near windows.

Everything an 80×24 Overworld shows SHALL still be visible: the landmark marks, monster wind-ups (the target tile highlighted and the monster flashing), damage numbers, bolts, the Nova ring, and the HUD panels, minimap and hotbar drawn over the scene. A tile's look SHALL depend only on its position and type, apart from animation over time.

#### Scenario: Stable look
- **WHEN** the hero walks away from an area and back, with animation time fixed
- **THEN** the area's grass, cobbles, trees and houses are drawn identically

#### Scenario: Wind-up stays readable
- **WHEN** a monster winds up an attack on the hero
- **THEN** the targeted tile is highlighted under the scene and the monster flashes, as in the terminal build

### Requirement: Smooth movement
The world simulation SHALL stay tile-based and deterministic. The window SHALL draw each creature between its previous and current tile, in proportion to how much of the move has passed, so a walk across tiles looks continuous. Bolts in flight SHALL be drawn the same way. The camera SHALL follow the hero's drawn position smoothly and stop at the map edges.

#### Scenario: Halfway through a step
- **WHEN** the hero is 4 ticks into an 8-tick move from tile (10, 5) to (11, 5)
- **THEN** the hero is drawn halfway between those two tiles

### Requirement: Window input
The window SHALL produce the same key events as the terminal: arrows, W/A/S/D, Enter, Esc, Backspace, Tab, printable characters, and left clicks at a canvas cell. Every key SHALL carry its press, repeat and release action. The Overworld SHALL turn a click into the map tile under the pointer. Closing the window SHALL behave like the Hub's Quit, asking first when there are unsaved changes.

#### Scenario: Held key without the fallback
- **WHEN** the player holds D on the Overworld in the window
- **THEN** the hero starts walking at once and keeps walking at the field pace until D is released

#### Scenario: Close with unsaved changes
- **WHEN** the window's close button is pressed after the game has changed since the last save
- **THEN** "Quit without saving?" is asked, and the window stays open on No

### Requirement: World-scale character art
Each hero path SHALL have world-scale sprites for the Overworld, separate from its battle art: 4 facings (down, up, left, right), each with a standing frame and 2 walk frames. Each training-monster tier SHALL have one world sprite. While walking, the hero SHALL alternate walk frames in step with its moves, and SHALL show the standing frame of its facing when stopped. A missing or malformed world sprite SHALL be reported at startup naming the file, and SHALL exit with code 2.

#### Scenario: Facing follows movement
- **WHEN** the hero walks left and then stops
- **THEN** the left-facing walk frames alternate during the walk, and the left-facing standing frame shows after it

### Requirement: Renderer independence and performance
Drawing SHALL never use or change the game's random stream or state; every look varies only by position hash and time. The Overworld scene SHALL render at 60 frames per second in a 1280×720 window on the reference machine (Apple M4 Pro, release build), measured and recorded.

#### Scenario: Drawing leaves the game untouched
- **WHEN** the Overworld scene is drawn 100 times
- **THEN** the game state, including its random stream, equals its value before

#### Scenario: Frame budget
- **WHEN** the starter area is rendered for 600 frames at 1280×720 in a release build
- **THEN** the average frame time is at most 16.6 ms, as recorded in `docs/benchmarks.md`
