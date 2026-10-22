# add-procedural-dungeons notes

## 2026-10-22: chunk 1, run without a stop for approval (the user's /goal)

SPEC DRIFT against `reference/dungeon-interfaces.md`, from how the game grew since it was written:
- **`EnemyBehavior::Basic` already existed** (the plain attack, and the fallback for a failed script), so 1.2 is a test only: a Basic enemy attacks once a turn and its battle is identical whatever the seed (the Battle has no RNG accessor, so "no draw" is shown that way).
- **No `medusa_pattern` behaviour:** enemies use Lua now, so the Gorgon Spawn has `"script": "scripts/medusa.lua"`.
- **`dungeon.json` has its own loader** (`loadDungeon`, like `loadWorld`) instead of joining `loadContent`; its errors are in the same `ContentError` form. Its validation tests are in `tests/dungeon_gen_test.cpp` (editing the shipped JSON), not fixture folders in `content_validation_test.cpp`. Mods already couldn't change `dungeon.json`.
- **`generateFloor` takes no `Content`**: generation only needs ids from the validated def.
- Extra validation beyond §2: `enemyCount.random` and `chestCount.random` must be at least 1 (they're `below()` bounds), and the elite must be on the last floor.
- Nothing loads the dungeon at startup yet; that comes with the screen (chunk 5).

Cross-platform determinism (1.5): the fixtures match on clang (macOS) and GCC 14 (Linux). Windows is only syntax-checked here (mingw); CI's MSVC job is the real check.

Verification: clang debug, asan (UBSan clean) and ci 605/605; window-off 570/570; GCC 14 Release `-Werror`; mingw syntax check clean. Floor generation is 15.8 µs per floor in release (docs/benchmarks.md).

## Chunk 2: field of view

- `computeFov` follows §5 exactly. The pillar fixture was worked out with an independent Python version of Ford's published algorithm using exact `Fraction`s (not the C++ under test), then checked by hand: the pillar shades only the tiles straight behind it, and the tiles beside the shadow sit exactly on the ±1/6 slope boundaries, which symmetric shadowcasting counts as visible.
- Verification: clang debug, asan (UBSan clean) and ci 610/610; window-off 575/575; GCC 14 Release `-Werror` 610/610 (after fixing a range-loop copy it flagged); mingw syntax check clean.

## Chunks 3 and 4: expedition rules, encounters and saves

SPEC DRIFT:
- **`step` and `startExpedition` take the `DungeonDef`** (it holds the floor rules, sight radius and chest tables), and `startExpedition` doesn't need `Content`.
- **`encounterEnemy` returns the scaled `EnemyDef`** instead of `encounterCombatant` returning a `Combatant`: the Battle is built from an `EnemyDef` (reference §6 says so too), and `enemyCombatant` turns one into a Combatant when needed.
- **Saves were already at version 3** (blessings), so the expedition makes them **version 4**: versions 1–3 load with no expedition, and 5 or higher is `NewerVersion` (the reference said 2 and 3).
- **The save holds the expedition as plain data** (`GameState::expedition`, an `ExpeditionSave`), not the live `Expedition`, so the save code doesn't depend on the dungeon code. `dungeon::fromSave` rebuilds the live one and returns nothing when it doesn't fit the dungeon (unknown floor, wrong map size, positions off the open tiles, a different number of chests).
- Messages: "You found {qty} {item name}.", "Your bag is full: you leave {qty} {name} behind.", "You go down to floor {n}.", "There are no stairs here.", "You defeated the {name}."

Verification: clang debug, asan (UBSan clean) and ci 628/628; window-off 593/593; GCC 14 Release `-Werror` 628/628; mingw syntax check clean.

## Chunk 5: the Expedition screen

- **5.1 and 5.2 are built but left unticked:** both say "verify manually", which needs a person at the terminal. Automated stand-ins pass (`tests/expedition_screen_test.cpp`): opening floor 1, save then Continue resuming the same map, a fight opening the Battle and fleeing back to the same spot, and falling back to the Hub. Still to try by hand: a chest, descending, the camera at the map edges, winning a fight, and that there's no way back up past floor 1's stairs.
- SPEC DRIFT:
  - The Hub gains Expedition as row 4, so Town, Save and Quit move to 5–7 (number keys and tap rows in the tests updated). Expedition says "In progress" when the game holds a run.
  - `BattleScreen` gained a second constructor for battles outside missions: a title, the enemy's sprite, and a callback that applies the outcome when the battle ends. It pops back to the map afterwards.
  - `resolveEncounter` also takes the consumables used in battle out of the bag (as `finishMission` does), and `encounterBattle` builds the battle with the hero's carried consumables.
  - The screen's Save writes straight away when this session owns the save (or none exists), and otherwise says to save from the Hub, which asks before replacing.
- **6.1 (CI green on all jobs) waits for a push**, which the user does per chunk.
- Verification: clang debug, asan (UBSan clean) and ci 632/632; window-off 597/597; window display tests 3/3; GCC 14 Release `-Werror` 632/632; mingw syntax check clean.
