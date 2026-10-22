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
