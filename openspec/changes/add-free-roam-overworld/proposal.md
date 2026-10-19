# Proposal

## Why

The game is played through menus. You pick "Training" or "Mission" and watch a fight. The user wants it to feel like the ASCII action RPGs they've seen, Spawn's browser MMO being the reference (2026-10-19): you walk a glyph-drawn world with WASD, walk up to people and press E, and fight monsters in real time with a swing, a couple of cooldown spells and a potion key. The engine already runs a 60 Hz fixed-step loop with non-blocking input and a diffing framebuffer (`src/engine/loop.hpp`), so this is new gameplay on the existing engine, not a new engine.

## What Changes

- **Explore.** The Hub gets a new first entry, "Explore", that opens the **Overworld**: one hand-authored map drawn in glyph-ramp terrain, filling the terminal. Esc returns to the Hub, where Save and Quit already live.
- **Real-time movement.** WASD or the arrow keys walk the hero tile by tile, with collision against trees, water and buildings. Holding a key keeps walking. Terminals that can report key release are used for this: the kitty keyboard protocol on macOS/Linux, and native key-up events on Windows. Elsewhere the OS key repeat drives it.
- **The world is the menu.** Town services are NPCs and buildings: walking up to one shows "press E", and E opens that service's existing screen. A cave entrance opens Training, and four boss lairs open the existing mission intro and turn-based battle, gated by each mission's level as today.
- **Field combat.** Monsters wander level-banded zones. They chase you when you come near and wind up visibly before they hit, so you can step out of the way.
  - You fight with **Swing** (Space, or click a cell to swing toward it), **Firebolt** (1, a projectile) and **Nova** (2, a burst around you). Each has a Fervor cost and a cooldown, and **Q** drinks a potion.
  - Damage uses the existing rule (power − DEF, minimum 0).
  - Monster stats, gold, EXP and drops come from the existing training-monster and drop rolls for the zone's level, and EXP goes through `grantExp`, so level-ups still give blessing points.
  - The hero's Life stat regenerates HP over time, as it does between training exchanges.
  - Falling in the field is a defeat like a lost training round: the hero loses that monster's EXP reward, is dead until revived at the Prior, and is sent back to the Hub.
- **HUD.** A status panel (name, level, HP, Fervor and EXP bars, gold, potions, weapon), a goal panel (the Hub's "Next:" line plus the nearest "press E" prompt), a minimap toggled with M, and a hotbar showing keys, costs and cooldowns.
- **Bosses stay turn-based.** The fun-pass bosses, their scripts, telegraphs and balance band are unchanged. The overworld is how you reach them.
- **No save change.** Entering the overworld always starts at the plaza, and monsters respawn. Everything else the field changes (HP, Fervor, gold, EXP, items) is already in the save.

## Capabilities

### New Capabilities
- `overworld`: the world map and its data, movement and collision, camera, landmarks and interaction, the HUD and the Hub's Explore entry.
- `field-combat`: field monsters (zones, spawning, chase, telegraphed attacks), the hero's field abilities, damage, rewards and defeat.

### Modified Capabilities
- `terminal-rendering`:
  - Keyboard controls: on the Overworld, the movement keys move the hero, and a held key keeps moving it.
  - New: key press and release reporting where the terminal supports it.
  - Large layout: the Overworld uses the whole terminal (80×24 up to 200×60) instead of the fixed 120×40.

## Impact

- **Game logic (new, headless):** `src/game/world/`, covering the map loader, the world simulation (fixed-step, integer ticks, events out) and field combat. No platform headers, and no I/O apart from loading the map files.
- **Engine/platform:**
  - `input.*` decodes kitty `CSI … u` key events with press/repeat/release.
  - `terminal_posix.cpp` asks the terminal whether it supports the protocol and turns it on only while the Overworld is open.
  - `terminal_windows.cpp` passes key-up events through.
  - A small held-key tracker.
- **App:** the new `screens/overworld.*` and the Hub's Explore entry. The landmark screens are the existing Town service, Training and Mission screens, reused unchanged.
- **Data/art:** `data/world/overworld.txt` (the map), `data/world/world.json` (tile legend, landmarks, zones, spawn point) and `data/world/field.json` (ability and monster tuning).
- **Not affected:** save format, battle engine, Lua scripts, the balance band, golden vectors.
- **Depends on:** `fun-pass` being archived first (the goal panel uses `nextGoal`, and level-ups grant blessings).
- **Relationship to `add-procedural-dungeons`:** independent. That change is a turn-based grid roguelike, and its non-goals rule out real-time movement. Whether its dungeons should become real-time and reuse this world simulation is an open question for the user (see design).
- **No new third-party dependencies.**
