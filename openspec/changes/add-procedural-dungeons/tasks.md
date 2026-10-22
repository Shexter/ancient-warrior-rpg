# Tasks

The reference is `reference/dungeon-interfaces.md` (abbreviated "ref" below). The reading list per chunk is in `design.md`.

## 1. Data and Floor Generation

- [x] 1.1 Add `data/dungeon.json` (ref §2), the 5 enemies plus the `basic` behavior value in `data/enemies.json` (ref §3), and their validation; verify `tests/content_validation_test.cpp` gains failing fixtures for an unknown pool enemy, an uncovered floor, and min > max, each named in the error
- [x] 1.2 Add `EnemyBehavior::Basic` to `Battle` (ref §3); verify a battle test that a Basic enemy attacks every turn, makes no RNG draw (the Rng state is unchanged after its turns), and that core golden scenarios A–D still pass
- [x] 1.3 Implement `src/game/dungeon/floor` steps 1–4 (BSP, rooms, corridors, entry and stairs) and `bfsDistances` and `renderAscii`; verify tests for the invariants in ref §4 across 10,000 seeds (connectivity, stairs present, border walls) and that two generations with the same seed are identical
- [x] 1.4 Implement steps 5–6 (elite, enemies, chests) and floor scaling; verify tests for no shared tiles, the safe radius, the elite only on floor 5, and that the average scaled enemy HP on floor 5 is greater than on floor 1 over 1,000 seeds
- [x] 1.5 Generate `renderAscii(generateFloor(seed 1234, floor 1))` and `(seed 1234, floor 5)`, commit them as `tests/fixtures/dungeon/seed1234_f1.txt` and `_f5.txt`, and add a test comparing against them; verify the test passes on all three CI platforms (cross-platform determinism)
- [x] 1.6 Add `--bench-dungeon N` to `heroes-sim` (generates N floors, prints mean µs per floor); verify the release build mean is under 5 ms and record it in `docs/benchmarks.md`
- [x] 1.7 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 2. Field of View

- [x] 2.1 Implement `src/game/dungeon/fov` per ref §5 with integer fractions and a `floorDiv` helper; verify `tests/fov_test.cpp`: the origin is always visible; open-room radius-8 visibility equals the set with `dx² + dy² ≤ 64`; the hand-made `fov_pillar.txt` fixture matches; and the symmetry property holds on 200 random floors × 50 random pairs
- [x] 2.2 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 3. Expedition Movement and Pursuit

- [x] 3.1 Implement `Expedition`, `startExpedition`, and `step()` for moves, wall bumps, wait, FOV update, and explored memory (ref §6); verify tests that a wall bump passes no turn (enemy positions unchanged) and that explored only grows
- [x] 3.2 Implement the enemy phase (wake on sight, sleepTurns, adjacency encounter, BFS step with N/E/S/W tie order, occupied tiles blocking); verify tests on a hand-built `Floor`: an enemy 5 steps away around a corner is adjacent after the hero waits 4 turns and triggers an encounter on the 5th; an enemy with no path stays put; two enemies in a corridor don't overlap
- [x] 3.3 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 4. Encounters, Loot, Floors, and Saves

- [x] 4.1 Implement chest opening (weighted pick + qty from `Expedition::rng`), `UseStairs` (descend, leave on floor 1, the no-stairs message), and the dead-hero gate; verify tests: leaving with loot keeps the items; a full bag reports `lostBagFull`; descending lands on the next floor's entry; a dead hero cannot start
- [x] 4.2 Implement `encounterCombatant` and `resolveEncounter` for all four outcomes; verify tests: hero bump and enemy approach both produce `encounterEnemy`; victory removes the enemy and grants scaled rewards; defeat sets HP 0, reduces exp, and keeps gear; flee puts the enemy to sleep for 3 turns
- [x] 4.3 Implement save v2 (ref §8); verify a round trip mid-floor-2 (position, explored, moved enemies, an opened chest, rng) compares equal, a checked-in v1 fixture still loads with no expedition, and `"version": 3` returns `NewerVersion`
- [x] 4.4 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 5. Expedition Screen

- [ ] 5.1 Add "Expedition" to the Hub menu (after Mission, with the same disabled reasons as Training) and implement `expedition_screen` per ref §7 (viewport, glyphs, lit and remembered colors, status rows, keys, Esc menu); verify manually: walk floor 1, open a chest, descend to floor 2, save, quit, Continue resumes on floor 2, climb back is not possible, and the camera follows at the map edges
- [ ] 5.2 Wire encounters to the Battle screen and back; verify manually that winning a fight returns to the same map position with the enemy gone, and that losing returns to the Hub with HP 0
- [x] 5.3 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 6. Final Verification

- [ ] 6.1 Verify CI is green on all jobs (including asan) and `openspec validate add-procedural-dungeons --strict` passes; list any SPEC DRIFT
- [ ] 6.2 Complete chunk quality check against `docs/engineering/code-correctness.md`.
