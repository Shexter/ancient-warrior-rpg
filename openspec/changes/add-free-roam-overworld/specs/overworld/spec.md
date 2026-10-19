# Spec Delta

## Purpose

A hand-authored, glyph-drawn world the hero walks around in real time. Town services, training and missions are places on the map, and the HUD keeps the hero's state and next goal in view.

## ADDED Requirements

### Requirement: Explore entry
The Hub SHALL list "Explore" as its first entry. It SHALL be disabled, with the same reason the Hub gives for Training, when the hero cannot adventure (dead, or without a weapon and armor). On the phone layout it SHALL be disabled with "Needs an 80×24 screen". Choosing it SHALL open the Overworld with the hero on the map's spawn tile, facing down. Esc on the Overworld SHALL return to the Hub, and the hero's HP, Fervor, gold, EXP and items SHALL be as the field left them.

#### Scenario: Entering the world
- **WHEN** an equipped, living hero chooses Explore on the Hub
- **THEN** the Overworld opens with the hero on the spawn tile

#### Scenario: Dead hero
- **WHEN** the hero has 0 HP and views the Hub
- **THEN** Explore is disabled with the same reason Training shows

#### Scenario: Re-entering starts at spawn
- **WHEN** the player walks away from spawn, presses Esc, and chooses Explore again
- **THEN** the hero is back on the spawn tile

### Requirement: World data
The world SHALL be loaded at startup from a text map and a data file that defines the tile legend (each tile character's terrain and whether it can be walked on), the spawn tile, landmarks and monster zones. A malformed world SHALL be reported at startup, naming the file and the 1-based line where there is one, and the game SHALL exit with code 2 before changing the terminal. A world is malformed when any of these hold:
- a map row has a different length from the first row;
- a map character is not in the legend;
- the spawn tile or a landmark sits on a tile that can't be walked on;
- a landmark names an unknown service, mission or training entrance;
- a zone lies outside the map;
- a landmark cannot be reached on foot from the spawn tile.

#### Scenario: Unknown tile character
- **WHEN** line 12 of the map contains a character the legend does not define
- **THEN** startup fails with an error naming the map file and line 12, and the exit code is 2

#### Scenario: Unreachable landmark
- **WHEN** a landmark is enclosed by water with no walkable path from spawn
- **THEN** startup fails with an error naming that landmark

### Requirement: Movement and collision
The hero SHALL move one tile at a time in the four cardinal directions, taking a fixed time per tile set in the field data. A movement key press SHALL move the hero exactly one tile and turn them to face that way. While a movement key is held, the hero SHALL keep moving at that pace. A move into a tile that can't be walked on, a monster or the map edge SHALL leave the hero in place, turned to face it. Releasing the key SHALL stop the hero at the end of the current tile.

#### Scenario: Tap moves one tile
- **WHEN** the player presses and releases D once on open ground
- **THEN** the hero ends one tile to the right, facing right

#### Scenario: Held key walks
- **WHEN** the player holds D for one second on open ground and the pace is 8 tiles per second
- **THEN** the hero has moved 8 tiles to the right, give or take one tile

#### Scenario: Blocked by water
- **WHEN** the hero faces a water tile and the player presses that direction
- **THEN** the hero does not move and faces the water

### Requirement: Deterministic simulation
The world SHALL advance in fixed steps. Given the same game state, world data and per-step inputs, it SHALL produce the same hero position, monsters, events and game state, independent of frame rate or rendering. The world SHALL fork the game's random stream once on entry and draw from nothing else.

#### Scenario: Replay
- **WHEN** the same recorded input sequence is fed twice to the world from the same state
- **THEN** both runs end in identical states and emit identical events

### Requirement: Camera and canvas
The Overworld SHALL draw on the whole terminal, from 80×24 up to 200×60, and centre it if the terminal is larger. The view SHALL follow the hero, scrolling only when the hero leaves a central box, and SHALL stop at the map edges. Terrain SHALL be drawn with glyph-ramp characters and colors chosen per tile type, and the character chosen for a tile SHALL depend only on its position and type, so revisiting an area looks the same.

#### Scenario: Map edge
- **WHEN** the hero stands at the map's left edge
- **THEN** the view's left column is the map's first column, and nothing is drawn beyond it

#### Scenario: Stable terrain
- **WHEN** the hero walks away from an area and back
- **THEN** every tile in that area is drawn with the same character and color as before

### Requirement: Landmarks and interaction
A landmark SHALL occupy one tile and open one of: a town service (Almonry, Forge, Prior, Thesaurus), Training, or a mission. When the hero stands on or next to a landmark, the goal panel SHALL show its name and "press E", and E SHALL open that screen. Leaving that screen, including a mission's result, SHALL return to the Overworld with the hero where they were. If the hero can no longer adventure at that point, the Overworld SHALL return to the Hub instead. A mission landmark SHALL show "Requires Lv N" or "Cleared" instead of "press E" when the Mission screen would disable that mission. In that case E SHALL do nothing but repeat that note. A mission landmark that can be started SHALL open that mission's intro and its battle exactly as the Mission screen does.

#### Scenario: Visiting the Forge
- **WHEN** the hero stands next to the Forge and the player presses E
- **THEN** the Forge screen opens, and Esc returns to the Overworld with the hero in the same tile

#### Scenario: Locked lair
- **WHEN** a level 12 hero stands at Avaritia's lair (minimum level 15) and presses E
- **THEN** no battle starts and the goal panel shows "Requires Lv 15"

### Requirement: HUD
The Overworld SHALL show these, drawn over the map:
- a status panel: hero name and level, HP, Fervor and EXP bars with numbers, gold, potion count and weapon name;
- a goal panel: the Hub's "Next:" line, or the nearest landmark prompt when there is one;
- a hotbar on the bottom row: each ability's key, name and Fervor cost, dimmed with its remaining cooldown while recharging, and the potion key;
- a minimap of the whole map with the hero and landmarks marked, which M shows or hides. It is hidden by default when the canvas is narrower than 120 columns.

#### Scenario: Toggling the minimap
- **WHEN** the player presses M on a 150×45 terminal
- **THEN** the minimap disappears, and pressing M again shows it

#### Scenario: Ability recharging
- **WHEN** Nova was cast less than its cooldown ago
- **THEN** the hotbar shows Nova dimmed with the seconds remaining
