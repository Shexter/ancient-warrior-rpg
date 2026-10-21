# Design

## Context

See proposal.md for why. The engine pieces this builds on already exist:

- `engine::Loop` runs fixed 60 Hz steps with non-blocking `readKeys()` and diffs a framebuffer against the last frame (`src/engine/loop.hpp`, `renderer.*`).
- Screens are a stack (`app::Screen`, with `Push`, `Pop` and `Replace` transitions). Each screen picks its own canvas size through `canvasSize()`. The loop accepts any size from 80×24 up to the terminal's size, or the portrait canvas.
- Input is key-down only. `InputDecoder` turns POSIX bytes into `KeyEvent`s (arrows, Enter, Esc, Backspace, Tab, printable characters, SGR left clicks). Windows reads key-down events only. Nothing reports a key release.
- `Framebuffer` cells hold a `char32_t` glyph with foreground and background colors. Glyph-ramp art already exists for sprites (`GlyphSprite`, `tools/` bake).
- Game logic is headless and deterministic: `Battle` and `TrainingSession` take actions and return events, and draw only from a forked `Rng`. The reusable rules are:
  - `rollTrainingMonster(rng, level)` and `rollDrop(rng, content, level)` for monsters and drops;
  - `grantExp` for EXP (level-ups grant blessing points after `fun-pass`);
  - `useConsumable` for potions;
  - `heroCombatant` for stats with equipment;
  - `canAdventure` for whether the hero can fight;
  - `nextGoal` (from `fun-pass`) for the goal line.
- A lost training round sets HP to 0 and subtracts that monster's EXP reward. The Prior revives the hero.
- The mission flow is `MissionSelect → (Replace) Intro → Battle → Result → Pop`, so the Result pops back to whatever pushed the first mission screen.

## Goals / Non-Goals

**Goals:**
- The world simulation is a headless, deterministic class, tested by feeding per-step inputs. The screen only translates keys into inputs and draws the result.
- The only engine change is input. Held keys come from press/release events where the terminal has them, and degrade to "one step per press or repeat" where it doesn't.
- It reuses existing rules and screens instead of writing parallel ones.

**Non-Goals:**
- Multiplayer, browser builds, or a 3D view.
- Diagonal movement, smooth sub-tile positions, or mouse-move aiming. Aiming is the facing direction, or toward a clicked cell.
- Real-time boss fights. Missions stay turn-based.
- Saving the world position or monster state.
- Mod overrides of the world map.
- A portrait/phone version of the Overworld (Explore is disabled there).
- Animated terrain such as water shimmer. It would rewrite every water cell several times a second (see the bandwidth risk). Add it when there's a measured budget for it.

## Decisions

### D1. A `World` class in `src/game/world/`, stepped in integer ticks
`World` owns the hero's tile, facing and timers, the monsters, the projectiles and its forked `Rng`. It holds a reference to the `GameState` and `Content`, and changes the hero only through existing `game::` functions. Two calls:
- `step(const WorldInput&)` advances exactly one 1/60 s tick and returns `std::vector<WorldEvent>`.
- `WorldInput` is `{ std::optional<Dir> move; bool swing, bolt, nova, potion, interact; std::optional<Tile> aim; }`. `move` means a movement key is held or requested this tick.

Every duration in the data (pace, wind-up, cooldowns, regen interval, respawn) is stored as ticks, converted once at load. Nothing in the simulation uses floating point.
*Alternative:* put the logic in the screen and use `dtSeconds`. That was rejected because it breaks replay determinism and makes the world untestable without a terminal.

### D2. Tile movement, cardinal only
The hero occupies one tile. A move starts only when the current one has finished (`ticksPerTile` from the data, 8 tiles/s to start). The hero is drawn on the destination tile once the move begins. That makes input feel instant, and the pace limits how fast moves can repeat. Monsters and the hero block each other.
*Alternatives:* sub-tile positions with interpolation, or 8 directions. Both look smoother, but diagonals need two held keys (impossible on plain terminals) and add corner-cutting rules. Sub-tile positions also need fixed-point math everywhere. Revisit after playtesting.

### D3. Key events: kitty protocol, Windows key-up, and a press/repeat fallback
- `KeyEvent` gains `enum class KeyAction { Press, Repeat, Release } action = Press`. Existing screens are untouched: the `App` passes them `Press` and `Repeat` as ordinary keys and drops `Release`, which matches what a plain terminal does today. Only the Overworld sees `Release`.
- **POSIX detection:** at startup, send `CSI ? u` followed by `CSI c` (primary device attributes). A `CSI ? <flags> u` reply arriving before the DA reply means the terminal supports the protocol. Wait at most 200 ms. This is the protocol's documented detection method, and every terminal answers DA, so nothing hangs.
- **POSIX enable:** when the Overworld becomes the top screen, push flags `1|2|8` with `CSI > 11 u` (disambiguate, report event types, all keys as escape codes). Pop with `CSI < u` when it stops being the top screen, and in the existing restore path (`restoreTerminal`, `atexit` and the signal handler). The restore path sends the pop only when a static flag says the push happened, because `CSI < u` must not reach terminals that never answered the query.
- **POSIX decoding:** `InputDecoder` decodes `CSI <code>[:…] [; <mods>[:<event>]] u` and the `CSI 1 ; <mods>:<event> A–D` arrow forms into the same `Key` values, plus the action. Enter (13), Esc (27), Backspace (127) and Tab (9) map to their keys.
- **Windows:** `KEY_EVENT_RECORD.bKeyDown == FALSE` becomes `Release`, and a held key's later down events become `Repeat`.
- **Fallback:** with no release reporting, every `Press` or `Repeat` of a movement key sets `move` for that tick only. The pace already stops a burst from out-running the walking speed. A plain terminal therefore has a short pause on hold (the OS repeat delay) and then walks at the repeat rate or the pace, whichever is slower.
- **Held state:** a small `HeldKeys` helper in `app/` tracks held directions. The most recently pressed held direction wins. In fallback mode it only reports this tick's presses.

*Alternative:* infer holding from repeat timing with a timeout on a plain terminal. That was rejected: the OS repeat delay (250–600 ms) is longer than any timeout that keeps a tap to one tile.

### D4. World data files
- `data/world/overworld.txt` is the map. Each row is the same length, and each character is a tile key.
- `data/world/world.json` holds:
  - the legend: `char → { terrain, walkable }`;
  - the spawn tile;
  - landmarks: `{ name, x, y, opens: { service: "almonry"|"forge"|"prior"|"thesaurus" } | { training: true } | { mission: "<id>" } }`;
  - zones: `{ name, x, y, w, h, level, count }`.
- `data/world/field.json` holds the tuning: pace, monster speeds, notice radius, wind-up, monster attack cooldown, regen interval and rates, respawn delay and minimum respawn distance from the hero, and each ability's key label, cost, cooldown, power multiple (in percent, an integer) and range or radius.
- Respawn uses a distance from the hero, not the screen's view (decided with the user, 2026-10-21): the headless world doesn't know the terminal size, and on a big terminal the whole starter map is visible, so "outside the view" would mean monsters never come back.

The world is loaded by the same JSON helpers and error style as `content_loader`, into a `WorldDef`, not into `Content`. Validation, including flood-fill reachability of every landmark from spawn, runs at load and fails with the file and line. The world is loaded in `main` beside the sprites and passed in through `Session`.
*Alternative:* put the world in `Content` so mods could override it. That was rejected because mod support for the world is a non-goal, and keeping it out leaves `Content` and the mod-merge rules unchanged.

### D5. Monster pursuit with a breadth-first distance map
When the hero changes tile, one BFS from the hero's tile over walkable tiles gives every chasing monster its next step, the neighbour with a smaller distance. The BFS is capped at twice the notice radius. Wandering is a random walkable step every N ticks, staying inside the zone. A zone holds at most about a dozen monsters, so they live in a plain `std::vector<Monster>`.
*Alternatives:* greedy steps toward the hero, which get stuck behind trees, or A* per monster, which is more code for no gain at this size. This is the same choice as D3 in `add-procedural-dungeons`, and the two can share a helper if both land.

### D6. Telegraphed hits as an explicit monster state
Each monster cycles through `Wander → Chase → WindUp(target tile, ticks left) → Cooldown`. Wind-up records the hero's tile when it starts. When it ends, the hit lands only if the hero is still on that tile. The screen draws the monster's glyph in a warning color, and marks the target tile, for the whole wind-up. That telegraph is what makes stepping away a real choice.

### D7. Abilities as data plus one resolver per kind
The abilities are Swing (melee, faced tile), Bolt (a projectile entity that moves N tiles per tick and stops at blocked tiles or the first monster) and Nova (radius by Chebyshev distance). Their numbers come from `field.json`, and the three kinds are three small functions. Power is `heroCombatant(...).atk × percent / 100`. Damage is `max(0, power − def)`, the same rule as `Battle::hit`'s basic attack, written as a shared free function so both use it. Field combat has no Guard, Reflect or statuses.
*Alternative:* map the existing turn-based skills (Smite, Sanctuary and the rest) to real time. That was rejected for now: their statuses count turns, and turning turns into seconds is a balance project of its own.

### D8. Reusing screens from landmarks
The Overworld `Push`es the existing screen: `MerchantScreen` for Almonry, `BlacksmithScreen` for Forge, `GuildmasterScreen` for Prior, `BankScreen` for Thesaurus, `TrainingSetupScreen`, and `MissionIntroScreen` for a mission. The pushed screen's `Pop` returns to the Overworld. Two small changes are needed:
- `MissionIntroScreen`'s "back" currently `Replace`s itself with `MissionSelectScreen`. It gains a flag so that, opened from the world, it `Pop`s instead.
- When the Overworld becomes the top screen again, it checks `canAdventure` and pops to the Hub if the hero fell (a lost mission or training round).

### D9. Rendering
- **Terrain:** each tile is drawn with a glyph from its terrain's ramp (for example grass `. , ' : ;`, tall grass `i l v Y`, trees `Y T ^`, water `~ -`, path `. :`, walls `# =`). It is picked by `hash(x, y) % ramp.size()`, with a small brightness jitter on the terrain's base color from the same hash. The look is stable by construction.
- **Entities:** the hero is `@` in the hero color, monsters are their name's first letter, and landmarks are a bright letter or `M`/`!` marker. Bolts are `*`, damage numbers float up for 0.6 s, and the Nova burst is a ring of `*` for 0.2 s.
- **Camera:** a dead zone a third of the view wide and high. The view clamps to the map, and a map smaller than the view is centred.
- **HUD:** framed panels with a darkened background, drawn over the map. Their layouts are fixed per canvas width band (80 to 119 and 120 and up), so they're easy to golden-test.
- **Canvas:** `canvasSize(terminal)` returns `clamp(terminal, 80×24, 200×60)`, or 80×24 under `--layout compact`.

### D10. Defeat bookkeeping
The world remembers which monster landed each hit on the hero. When HP reaches 0 it subtracts that monster's EXP reward (not below 0) and emits `HeroFell`, and the screen pops to the Hub with the message. This mirrors the training defeat rule in `training.cpp`.

## Risks / Trade-offs

- **Terminal bandwidth:** scrolling a 200×60 view rewrites most cells, around 12k cells × ~20 bytes, up to 8 times a second, which is about 2 MB/s. That's fine locally but heavy over SSH. → The dead-zone camera scrolls only at the box edge. A task measures the bytes per scroll, and if it's too slow the camera moves in jumps of 4 tiles.
- **Kitty mode left on after a crash:** → The pop is in the same signal-safe restore path as the alternate screen, guarded by a static flag. A test checks that the leave sequence includes it only after a push.
- **Plain-terminal hold feels sticky** (the repeat delay before walking starts). → Accepted and documented. Kitty-protocol terminals (kitty, WezTerm, Ghostty, foot, iTerm2 3.5+, Alacritty 0.13+) and Windows walk smoothly.
- **Field balance:** monsters from the training roll at the zone's level may be too easy or too hard in real time. → Every number is in `field.json`. A headless bot test (chunk 6) checks that a zone at the hero's level can be cleared by a simple dodge-and-swing policy.
- **Two map systems:** if `add-procedural-dungeons` lands turn-based, there are two kinds of map play. → Its BFS and tile helpers can be shared, and the user decides whether dungeons go real-time (open question).

## Migration Plan

There's no save or data migration. The change is additive behind the Hub's new Explore entry. Rolling it back means removing that entry. Nothing else depends on the world.

## Open Questions

- Should `add-procedural-dungeons` become real-time floors on this `World` simulation instead of turn-based? It changes that change, not this one.
- Once the field feels right, should bosses also fight in real time? That would be a future change. This one keeps them turn-based.
