# Overworld (`data/world/`, `src/game/world/`, `src/app/screens/overworld.*`)

## World data and simulation (`data/world/`, `src/game/world/`)

The Overworld is loaded at startup, after content and before sprites, by `world::loadWorld(dataDir, content)`. It reads three files and parses them with `parseWorld(map, worldJson, fieldJson, content)`. Errors are `ContentError`s named `world/overworld.txt`, `world/world.json` or `world/field.json`. Any error prints every problem and exits with code 2 before the terminal changes.
- **`overworld.txt`:** one character per tile, every row as long as the first (at most 1024×1024). A `\r` before each newline is ignored, and a final newline adds no row. Map errors carry the entry `line N` (1-based): a ragged row, or a character not in the legend (the first such character per line).
- **`world.json`:**
  - `legend`: one printable non-space character → `{terrain, walkable}`. The terrains are `grass`, `tall_grass`, `tree`, `water`, `path`, `sand`, `rock`, `wall` and `floor`.
  - `spawn`: `{x, y}`.
  - `landmarks`: `{name, x, y, opens}`, where `opens` holds exactly one of `service` (`almonry`, `forge`, `prior`, `thesaurus`), `training: true`, or `mission` (a mission id).
  - `zones`: `{name, x, y, w, h, level 1–30, count 1–50}`.
  - Placement checks run only when everything parsed. Spawn and landmarks must be on walkable tiles inside the map, zones inside the map, and every landmark reachable from spawn on foot (four directions).
- **`field.json`:** every key is required. Durations are 1–600000 ms, converted to the nearest whole tick at 60 ticks per second and at least 1 (125 ms → 8 ticks). The shipped values are:

  | Key | Shipped | Meaning |
  |---|---|---|
  | `heroStepMs` | 125 | time to walk one tile |
  | `monsterWanderMs` | 700 | between a wandering monster's steps |
  | `monsterChaseMs` | 250 | between a chasing monster's steps |
  | `noticeRadius` | 6 | tiles, 1–32 |
  | `windUpMs` | 500 | from a wind-up to its hit |
  | `attackCooldownMs` | 1200 | after a monster's hit |
  | `lifeRegenMs` | 2000 | between Life regen ticks |
  | `fervorRegenMs` | 500 | between +1 Fervor ticks |
  | `respawnMs` | 30000 | respawn delay |
  | `respawnDistance` | 12 | tiles, 0–1024 |
  | `boltStepMs` | 50 | a Firebolt's time per tile |
  | `swing` | cost 0, 350 ms, 100% | `{cost, cooldownMs, powerPercent}` |
  | `firebolt` | cost 15, 800 ms, 150%, reach 10 | plus `reach` (tiles of flight, 1–32) |
  | `nova` | cost 30, 4000 ms, 120%, reach 3 | `reach` is the radius |

The shipped map is a 72×30 starter area. It has the town plaza at spawn (35, 14), the Almonry, Forge, Prior and Thesaurus at the four building doors, the Catacombs (training) at the cave mouth to the north, the level 1 Glyphwick Meadow zone to the east, and the Lair of Avaritia inside it.

`world::World(def, state, content)` is the simulation. It starts the hero on spawn facing down, forks `state.rng` once, and places each zone's monsters. `step(WorldInput)` advances one tick (and does nothing once the hero has fallen):
- Once the current move has finished, a held or requested direction turns the hero that way.
- If the next tile is walkable and inside the map, the hero moves there at once and emits `HeroMoved{from, to}`. The next move can start `ticksPerTile` ticks later.
- A refused move still turns the hero.
- Monsters block the hero, and the hero blocks monsters.
- **Regen:** every `lifeRegenMs` the hero regains Life HP (base plus equipment, up to max HP; nothing at Life 0, e.g. a Fighter). Every `fervorRegenMs` the hero regains 1 Fervor, up to the maximum. Both counters start full on entry.
- **Monsters** (`Monster`: zone, `TrainingMonster` stats, HP, tile, mode, target, timer):
  - A new monster takes a random free walkable tile of its zone at least `respawnDistance` (Chebyshev) from the hero, then rolls its stats with `rollTrainingMonster` at the zone's level. The draw order is the tile, then atk, def and gold.
  - On entry each zone fills up to its count. A zone below its count gets one back every `respawnMs` while a tile qualifies; with none, it waits (an empty zone near spawn on a small map fills only once the hero walks away).
  - **Wander:** every `monsterWanderMs` it picks one of the four directions or stays (`rng.below(5)`), moving only onto a free walkable tile in its zone.
  - **Chase:** the hero is in the zone and within `noticeRadius` (Chebyshev), or within twice that once it is already chasing. Noticing shortens its timer to `monsterChaseMs` at once. Every `monsterChaseMs` it steps to the free in-zone neighbour with the smallest walking distance to the hero (ties in the order up, down, left, right). The distances come from one breadth-first search over walkable tiles in a window of twice the notice radius around the hero, redone only when the hero changes tile.
  - **WindUp:** when it is next to the hero (four directions) it targets the hero's tile for `windUpMs`. When the wind-up ends it hits only if the hero is still on that tile: `defendedDamage(atk, hero DEF + equipment DEF)`, emitting `Damaged{tile, amount, onHero}`.
  - **Cooldown:** `attackCooldownMs`, then Wander, which re-checks for a chase on its next tick.
- **Abilities**, each tick after the hero's move (all cooldowns first count down by one):
  - A click (`aim`, a map tile other than the hero's) turns the hero along the axis with the larger distance (horizontal on a tie), then swings.
  - Using an ability needs its cooldown at 0 and Fervor ≥ its cost. Otherwise it emits `AbilityRefused{ability}` and changes nothing.
  - Power is `(ATK + equipment ATK) × powerPercent / 100`, and each hit deals `defendedDamage(power, monster DEF)`.
  - **Swing** hits the monster on the faced tile, if any.
  - **Firebolt** launches a `Bolt` from the hero's tile in the facing direction. Every `boltStepMs`, starting that same tick, it moves one tile. It hits and vanishes at the first monster, vanishes at a tile that can't be walked on or the map edge, or after `reach` tiles.
  - **Nova** emits `NovaBurst{center, radius}` and hits every monster within the radius (Chebyshev).
- **Potions:** Q uses the first bag slot whose item `restoresHp` (`town.hpp`: a consumable that restores HP or fully restores), through `useConsumable`, and emits `PotionDrunk{itemId}`. With none it emits `NoPotions`.
- **A kill** (a monster at 0 HP) removes the monster and calls `grantMonsterRewards` (`training.hpp`, the same code training rounds use): gold, EXP through `grantExp` (so level-ups grant blessings), then a `rollDrop` at the zone's level that is lost if the bag can't take it. It emits `MonsterDefeated{name, tile, rewards}`. Bolts move before monsters act each tick.
- **Falling:** a hit that takes HP to 0 subtracts that monster's EXP reward (`loseToMonster`, not below 0), keeps equipment, gold and items, and emits `HeroFell`.
- `defendedDamage(power, def)` (`battle.hpp`, `max(0, power − def)`) is the one damage rule, shared by `Battle::hit`, training exchanges and the field.
- `nearbyLandmark()` is the landmark on or next to the hero, diagonals included. The smallest `|dx| + |dy|` wins, and a tie goes to the one listed first.
- `WorldInput::interact` (E this tick) emits `Interact{landmark index}` when there is one.

## Screen (`OverworldScreen`)

- **Opening and closing:** the Hub's first entry, **Explore**, pushes it. Explore is disabled with the Training reason when the hero can't adventure, with "Needs an 80×24 screen" when the last Hub frame was the phone layout, and with "No world loaded" if the session has no world (only possible in tests). Each opening builds a new `World`, so the hero starts on spawn every time. Esc (a press) pops back to the Hub, which keeps its selection.
- **Input:** the screen opts into key actions (`wantsKeyActions`). Each step feeds the keys to a `HeldKeys` and steps the world once with its direction. See `terminal.md` for plain terminals versus release reporting.
- **Canvas:** the terminal size clamped to 80×24–200×60, or 80×24 under `--layout compact`. Below 80×24 the loop falls back to its resize prompt.
- **Camera:** the map tile at the canvas's top-left. Drawing updates it, since only drawing knows the canvas size. In the window (`window.md`), `drawHud(frame, viewTiles)` follows the hero over the scene's view of tiles instead, and `sceneView()` hands the window the camera and the effects to draw. The first frame centres it on the hero. After that, on each axis, it moves only when the hero leaves the middle third of the view (`view / 3` cells from each side), and it clamps to the map. A map narrower or shorter than the view is centred, and the margin is the background color with blank cells.
- **Terrain:** each tile is drawn with a character from its terrain's ramp, picked by a hash of its position. Its foreground is the terrain's color at 88–112% brightness from the same hash, on the terrain's dark background. So a tile always looks the same:

  | Terrain | Ramp |
  |---|---|
  | grass | `` .,'`.. `` |
  | tall grass | `ilvY;:` |
  | tree | `YT^` |
  | water | `~-~=` |
  | path | `.:.` |
  | sand | `.:,` |
  | rock | `^nA` |
  | wall | `#=` |
  | floor | `.:` |

- **Landmarks** are drawn inverted so they can't pass for terrain: a lair is a light `!` on red, and any other landmark is its name's first letter on the accent color (A, F, P, T and C on the first map).
- **The hero** is `@` in the accent color over its tile's background.
- **Monsters** are drawn as their name's lower-case initial (`f` Fox, `b` Bat or Bull, `h` Horse) in a red. During a wind-up the target tile's background turns dark red and the monster flashes the accent color every 4 steps.
- **Bolts** are a gold `*`. The **Nova ring** is a blue `*` on every cell at exactly the radius (Chebyshev) for 0.2 s.
- **Notices** (the messages above) take the goal line's place for 2.5 s. The goal line shows a notice first, then a landmark prompt, then the Next line.
- **Hotbar** (the bottom row, panel color): `SPC Swing`, `1 Firebolt 15`, `2 Nova 30`, `Q Potion xN`. Each key is drawn inverted, and the cost is in the Fervor color. While an ability recharges its label is dim, with the seconds left ("Nova 30 3.0s"). A refused use flashes its label's background red for 0.3 s.
- **Damage numbers** rise from the `Damaged` events: one row above the tile for 0.3 s, then two rows for 0.3 s, red on the hero and gold on a monster.
- **Opening** marks the session unsaved, since the world forks the game's random stream.
- **Each step**, in order:
  - If the hero can't adventure (they fell in a mission or a training round opened from here) the screen pops to the Hub, which shows that screen's last message.
  - If a landmark's screen has just closed, the goal line is worked out again.
  - Then the keys are read (releases ignored).
    - Esc (a press) pops.
    - Space swings, 1 casts Firebolt and 2 casts Nova; these also take a held key's repeats, and the cooldowns pace them.
    - A click on a map tile aims and swings.
    - Q drinks, E interacts and M toggles the minimap (presses only).
  - The world steps. Each event does one thing:
    - `HeroFell` sets the Hub's message to "You fell in the field. Visit the Prior to be revived." and pops.
    - `Damaged` adds a damage number, and `NovaBurst` shows the ring.
    - `AbilityRefused` flashes that hotbar slot.
    - `NoPotions` shows "No potions" and flashes the potion slot; `PotionDrunk` shows "Drank Holy Water".
    - `MonsterDefeated` shows "Fox defeated · +G gold · +E exp", adding " · Level up! +N blessing(s) at the Prior", " · Drop: X" or " · Bag full, X lost" when they apply.
    - `Interact` opens the landmark.
- **Interacting** pushes the landmark's screen: the Almonry (`MerchantScreen`), Forge (`BlacksmithScreen`), Prior (`GuildmasterScreen`), Thesaurus (`BankScreen`), the Catacombs (`TrainingSetupScreen`), or a lair's `MissionIntroScreen` opened with `fromWorld`.
  - Those screens' Esc and their results `Pop` back here, with the hero where they stood. The Training and mission Result screens still say "Enter return to the Hub".
  - Backing out of a lair's intro (Esc, or No to "Enter the lair?") pops back here instead of replacing itself with Mission Select.
  - A lair whose mission `missionBlocker` refuses ("Cleared", "Requires Lv N") opens nothing.
- **HUD below 120 columns:** two rows over the top of the map, on the panel color.
  - Row 0: name, `Lv`, an HP bar (10 cells) with numbers, a Fervor bar (6) with numbers, gold, `!xN` (HP-restoring consumables in the bag) and the weapon name.
  - Row 1: `» ` followed by the landmark prompt in the accent color ("Forge — press E", or "Lair of Avaritia — Requires Lv 15" / "— Cleared"), or the Hub's "Next:" line when no landmark is near.
- **HUD from 120 columns:** a 40×7 status panel at (1, 1) (name and level; HP, FV and XP bars of 20 with numbers, XP being `exp` of `expToNext`; gold, potions and weapon). Below it, at (1, 9), a goal panel at least 40 wide, holding the same line.
- **The goal line** is `nextGoal`. It is computed when the screen opens and again after any landmark screen closes, never per frame.
- **Minimap:** a 26×12 panel (34×16 from 120 columns) in the bottom-left corner, one row up from the bottom.
  - It samples the map evenly: `~` water, `.` walkable, `#` blocked, each in its terrain color at 80%. Landmarks use their map marks, and the hero is `@`.
  - It shows by default only from 120 columns. M sets an explicit choice: the opposite of what the last frame showed.
- **Scroll cost:** see `docs/benchmarks.md` (at most about 233 KB per step at 200×60).
