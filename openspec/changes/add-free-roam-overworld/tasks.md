# Tasks

## 1. World data and movement (headless)

- [x] 1.1 Add `WorldDef` and its loader for `data/world/overworld.txt`, `world.json` and `field.json` (D4), converting durations to ticks. Validate row lengths, unknown tile characters, spawn and landmarks on walkable tiles, unknown services and missions, zones inside the map, and every landmark reachable from spawn by flood fill. Errors name the file and the 1-based line. Verify with loader tests for each malformed case in the World data requirement and one valid fixture.
- [x] 1.2 Add a small first map (town plaza, the four services, a cave entrance, one zone, one lair) to `data/world/`, and load it at startup beside the sprites. A malformed world exits with code 2 before the terminal changes. Verify that the game starts with the real data, and with a broken copy via `--data-dir` exits 2 with the file and line.
- [x] 1.3 Add `World` with `step(WorldInput)` returning `WorldEvent`s (D1, D2): hero tile, facing, pace in ticks, blocking by terrain and map edge, and one `Rng` fork on construction. Verify with tests for Tap moves one tile, Held key walks (8 tiles in 60 ticks ±1), Blocked by water, and Replay (the same inputs twice give equal states and events).
- [x] 1.4 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 2. Key press and release

- [ ] 2.1 Add `KeyAction { Press, Repeat, Release }` to `KeyEvent`, defaulting to `Press`. Teach `InputDecoder` the kitty `CSI … u` forms and the `CSI 1 ; mods:event A–D` arrows, with Enter, Esc, Backspace and Tab mapping to their keys. `App` drops `Release` and passes `Repeat` as a plain key to every screen that doesn't opt in. Verify with decoder tests for press, repeat and release of `w`, the arrows and Enter, and that the existing input and screen tests pass unchanged.
- [ ] 2.2 POSIX: detect the protocol at startup (`CSI ? u` then `CSI c`, 200 ms cap). Push `CSI > 11 u` while an opted-in screen is on top and pop it when it isn't. The restore path pops only if a push happened (D3). Windows: report key-up as `Release` and later down events as `Repeat`. Verify with tests of the leave sequence (the pop appears only after a push) and of the detection reply parser with and without a `CSI ? flags u` reply.
- [ ] 2.3 Add `HeldKeys` (the most recently pressed held direction wins, and fallback mode reports only this tick's presses). Verify with tests for overlapping holds, release, and the fallback mode.
- [ ] 2.4 Update SimpleDocs `terminal.md` with the key actions, detection, push and pop, and the fallback. Verify that it matches the code.
- [ ] 2.5 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 3. Overworld screen

- [ ] 3.1 Add `OverworldScreen`. It opts into key actions, turns keys into `WorldInput` through `HeldKeys`, and sizes its canvas as `clamp(terminal, 80×24, 200×60)` (80×24 under `--layout compact`). It draws terrain from per-type ramps chosen by position hash, the hero `@`, and the dead-zone camera clamped to the map (D9). Verify with screen tests for Overworld fills the terminal, Very large terminal, Map edge, Stable terrain, and a golden frame of the first map.
- [ ] 3.2 Add the Hub's first entry, Explore. It is disabled with the Training reason when the hero can't adventure, and on the phone layout with "Needs an 80×24 screen". Esc returns to the Hub, and re-entering starts at spawn. Verify with Hub tests for Entering the world, Dead hero, Re-entering starts at spawn, and that the number shortcuts and existing Hub tests are updated for the new first entry.
- [ ] 3.3 Measure the bytes written per scroll step at 200×60 and 120×40 with the diff renderer, and record them in `docs/benchmarks.md`. If a 200×60 scroll is over 300 KB, switch the camera to 4-tile jumps. Verify that the recorded numbers and the camera rule match.
- [ ] 3.4 Update SimpleDocs (`app.md`, plus a new `overworld.md` for the world data and simulation). Verify that they match the code.
- [ ] 3.5 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 4. Landmarks and HUD

- [ ] 4.1 Landmark interaction in `World`: emit the nearest landmark within one tile, and E emits `Interact`. The Overworld pushes the Almonry, Forge, Prior, Thesaurus, Training setup or Mission intro screen (D8). `MissionIntroScreen` gains a from-world flag so its back `Pop`s. On regaining the top, the Overworld pops to the Hub if `canAdventure` is false. Verify with tests for Visiting the Forge (Esc returns with the hero in the same tile), Locked lair ("Requires Lv 15", no battle), a cleared lair, a started mission's result returning to the Overworld, and a lost mission returning to the Hub.
- [ ] 4.2 HUD: the status panel (name, level, HP, Fervor and EXP bars, gold, potions, weapon), the goal panel (`nextGoal`, or the landmark prompt), and the minimap with M (hidden by default under 120 columns), in both width bands. Verify with screen tests for Toggling the minimap, the landmark prompt replacing the goal line, and golden HUD frames at 80×24 and 150×45.
- [ ] 4.3 Update SimpleDocs `overworld.md` and `app.md`. Verify that they match the screens.
- [ ] 4.4 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 5. Field monsters and defeat

- [ ] 5.1 Zones and monsters in `World` (D5, D6). Monsters are rolled with `rollTrainingMonster` at the zone level, wander inside the zone, notice within the radius, chase by a BFS distance map capped at twice the radius, and give up beyond it or outside the zone. Each does a wind-up on the hero's tile, then hits or misses, then cools down. Monsters and the hero block each other. Verify with tests for Zone level sets stats, Dodging, Standing still, a chase around a tree, and giving up outside the zone.
- [ ] 5.2 Damage as a shared `max(0, power − def)` free function used by `Battle`'s basic attack and the field. Life regen on its interval, Fervor regen, and floating damage numbers as events. Verify with tests for Armor absorbs a weak hit and both regens, and that the battle golden tests pass with `battle_golden_data.hpp` unchanged.
- [ ] 5.3 Field defeat (D10): subtract the last hitter's EXP reward (not below 0), keep equipment, gold and items, emit `HeroFell`, and the Overworld returns to the Hub with "You fell in the field. Visit the Prior to be revived." Verify with tests for Falling (Explore, Training and Mission disabled afterwards) and the EXP floor.
- [ ] 5.4 Respawn after the delay on a walkable zone tile outside the view. Verify with the Respawn out of sight test.
- [ ] 5.5 Update SimpleDocs `overworld.md`. Verify that it matches the code.
- [ ] 5.6 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 6. Abilities, potions and rewards

- [ ] 6.1 Swing (Space, or a click turning toward the clicked cell on its longer axis), Firebolt (1, a projectile that stops at blocked tiles or the first monster within range), and Nova (2, Chebyshev radius), with costs, cooldowns and power percents from `field.json` (D7). Refused uses change nothing and emit a flash event. Verify with tests for Swing at an adjacent monster, Not enough Fervor, Bolt blocked by a tree, a click aiming left, and the Nova radius edge.
- [ ] 6.2 Q uses the first HP-restoring consumable through `useConsumable`, or shows "No potions". Kills give gold and EXP through `grantExp` and a `rollDrop` at the zone level, and a drop that doesn't fit a full bag is lost with a message. Verify with tests for Drinking a potion, no potions, Level up in the field (a blessing point is gained), and a full bag.
- [ ] 6.3 Hotbar on the bottom row (key, name, cost, a dimmed cooldown countdown, the flash on refusal, the potion key), plus the bolt, the Nova ring and the wind-up warning color drawn. Verify with screen tests for Ability recharging, and golden frames of a wind-up and a Nova.
- [ ] 6.4 Add a headless field bot (step away during wind-ups, swing when facing a monster, potion below 30% HP). Add a test that a hero at each starter path's intended level clears a zone of that level without falling in at least 8 of 10 seeds, and tune `field.json` until it passes. Record the numbers in `docs/benchmarks.md`. Verify that the test passes and the numbers are recorded.
- [ ] 6.5 Update SimpleDocs `overworld.md`. Verify that it matches the code.
- [ ] 6.6 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 7. The full world

- [ ] 7.1 Author the full map: the plaza with all four services, a training cave, zones banded from level 1 near town to 30 at the edges, and the four lairs (Avaritia, Gula, Ira, Medusa) placed by mission level. Verify that the loader accepts it, and add a test that every lair is reachable and every zone level from 1 to 30 is covered.
- [ ] 7.2 Playtest manually in a kitty-protocol terminal, in macOS Terminal (fallback) and on Windows Terminal: walking, dodging, every landmark, and a lost fight. Record the findings in `notes.md` and fix what blocks play. Verify that the notes are written.
- [ ] 7.3 Complete chunk quality check against `docs/engineering/code-correctness.md`.
