# Tasks

Each chunk's reading list is in `design.md` → Implementation Handoff. Build and test commands, from the repo root: `cmake --preset debug && cmake --build --preset debug && ctest --preset debug`.

## 1. Toolchain and Skeleton

- [x] 1.1 Install CMake (`brew install cmake`) and verify `cmake --version` reports ≥ 3.25
- [x] 1.2 Add `.clang-format` and `.gitignore` (`build/`, `.DS_Store`, `heroes RPG/*.wav`, `heroes RPG/*.mp3`, `relay/.dev.vars`, `relay/node_modules/`, `__pycache__/`); verify `git status --ignored` lists the audio files as ignored
- [x] 1.3 Add `cmake/Dependencies.cmake` (nlohmann/json v3.12.0 and GoogleTest v1.17.0 by URL with `URL_HASH SHA256`, `SYSTEM`) and `cmake/Warnings.cmake` per `interfaces.md` §3 using the SHA-256 values listed in §3; verify a configure from an empty `build/` succeeds (a hash mismatch is a stop-and-report, not a fix)
- [x] 1.4 Add root `CMakeLists.txt` and `CMakePresets.json` (debug, release, asan, ci) with the targets in §2 as empty stubs: `heroes` prints `Ancient Warrior RPG 0.1.0` and exits 0, `heroes_tests` has `tests/smoke_test.cpp` asserting `1 + 1 == 2`; verify `cmake --preset debug && cmake --build --preset debug && ctest --preset debug` passes and `./build/debug/heroes` prints the version
- [x] 1.5 Verify the `asan` preset builds and its tests pass locally
- [x] 1.6 Add `.github/workflows/ci.yml` per §3; verify by pushing a branch that all four jobs (3 OS + asan) pass. If no remote exists yet, ask the user before creating one
- [x] 1.7 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 2. Deterministic RNG

- [x] 2.1 Implement `src/game/rng.{hpp,cpp}` exactly per §4, including `fork()`, `state()`/`fromState()`, and defaulted `operator==`
- [x] 2.2 Add `tests/rng_test.cpp` with one test per row of the RNG golden table, plus `below(1) == 0` always and a `fork()` test showing the master advanced by exactly 2 draws; verify all pass locally and on all three CI platforms
- [x] 2.3 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 3. Game Content

- [x] 3.1 Implement `src/game/content.hpp` types and `Content` lookups per §5
- [x] 3.2 Write `data/paths.json`, `items.json`, `skills.json`, `enemies.json`, `missions.json`, `recipes.json`, `story.json` transcribing `content-tables.md` exactly (item order as in the table); add a placeholder `assets/sprites/medusa.sprite` (any valid 2×2 sprite) so the sprite reference validates
- [x] 3.3 Implement `src/game/content_loader.{hpp,cpp}` with every validation rule in §5, collecting all errors; verify `tests/content_test.cpp`: shipped data loads; Fighter stats are 350/50/50/5/0; `iron_sword` atk is 50; item count is 20; drop candidates number 16
- [x] 3.4 Add `tests/fixtures/content_bad/<case>/` directories (one per validation rule, each a full copy of `data/` with one defect) and `tests/content_validation_test.cpp` asserting each produces an error naming the expected file and entry; include a two-defect case asserting both errors are reported
- [x] 3.5 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 4. Combat Engine (headless)

- [x] 4.1 Implement `src/game/errors.hpp`, `inventory.{hpp,cpp}`, and `hero.{hpp,cpp}` (`newGame`, `validateHeroName`, `grantExp`, `equipmentBonus`, `canAdventure`) per §6; verify `tests/hero_test.cpp` covers the four leveling vectors in §6, name validation (empty, 9 chars, trimmed, non-ASCII rejected), and Iron Sword at upgrade 2 giving 150 ATK
- [x] 4.2 Implement `src/game/events.hpp` and `battle.{hpp,cpp}` per §7; verify `tests/battle_test.cpp`: each `ActionError` returns with no events and the Battle unchanged; Flee ends with `Fled`; the absorbed hit emits `Damaged` with amount 0
- [x] 4.3 Add `tests/battle_golden_test.cpp` reproducing golden scenarios A–D event by event (write a small `toString(const Event&)` test helper producing the golden line format); verify all four pass on all CI platforms
- [x] 4.4 Add status-timing tests: Frozen Throne skips exactly 3 enemy turns and expires both statuses on the 3rd; re-applying a status refreshes and does not stack; a turn-cap test (hero and enemy both unable to deal damage) ends `Draw` at turn 1000
- [x] 4.5 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 5. Balance Simulator

- [x] 5.1 Implement `tools/sim/sim.{hpp,cpp}` (`SimConfig`, `Report`, `runSimulation`, both policies, threading) and `tools/sim/main.cpp` (argument parsing, exact output format) per §13
- [x] 5.2 Add `tests/sim_test.cpp`: thread counts 1 and 8 give equal Reports for seed 7 and 10,000 battles; a starting Fighter with the `attack` policy has a 0% win rate; an unknown item fails validation
- [x] 5.3 Run `./build/release/heroes-sim --path fighter --enemy medusa --battles 100000 --seed 7 --bench` and record the command, machine, and output in `docs/benchmarks.md`
- [x] 5.4 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 6. Progression and Town Services (logic only)

- [x] 6.1 Implement `src/game/training.{hpp,cpp}` (`rollTrainingMonster`, `rollDrop`, `runTraining`) per §8; verify `tests/training_test.cpp`: the monster and drop vectors from `golden-vectors.md`; an invalid cave level (0, 31) and rounds (0, 101) are rejected; death stops later rounds, sets HP 0, and deducts that round's exp; an empty drop table terminates
- [x] 6.2 Implement `src/game/town.{hpp,cpp}` merchant (`buy`, `sell`, `auction`/`rollAuction`) and bank; verify `tests/town_merchant_test.cpp`: the auction vectors; for each failure path, `GameState` before == after
- [x] 6.3 Implement blacksmith (`equip`, `upgrade`, `forge`/`rollForge`); verify `tests/town_blacksmith_test.cpp`: the forge vectors; an upgrade consumes L materials; equipping returns the old item at level 1; a full bag blocks equip and forge with state unchanged
- [x] 6.4 Implement guildmaster (`revive`, `useConsumable`, `buyCharisma`, `learnSkill`, `rename`) and `startMission`/`finishMission`; verify `tests/town_guild_test.cpp`: revive with 40 gold; charisma with 30 exp is refused with nothing spent; charisma at level 10 Fighter for 500 exp gives +10 for 1000 gold; learning Frozen Throne consumes the key and gold; mission Victory grants rewards and marks completed, Defeat destroys gear
- [x] 6.5 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 7. Save System

- [x] 7.1 Implement `src/game/save.{hpp,cpp}` per §10; verify `tests/save_test.cpp`: a round trip of a non-trivial state (bank open, items in both, skills, a completed mission, a mid-sequence RNG) compares equal with `==`
- [x] 7.2 Implement `src/platform/paths.{hpp,cpp}` with the `HEROES_SAVE_DIR` override; verify tests that corrupt JSON, a missing field, an unknown item ID, and `"version": 2` each return the right `SaveError` and leave the file byte-identical
- [x] 7.3 Verify a test that saves twice to the same path (overwrite) passes on the Windows CI job
- [x] 7.4 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 8. Terminal Platform and Framebuffer

- [x] 8.1 Implement `src/engine/color`, `framebuffer`, and `renderer` per §12; verify `tests/renderer_test.cpp`: the first present writes all cells; an identical second frame writes 0 cells; a one-cell change writes exactly 1 cell, and its bytes contain one cursor move; the Ansi256 mapping is pure red → 196, (16,16,24) → 16
- [x] 8.2 Implement `src/engine/input` (`InputDecoder`); verify `tests/input_test.cpp`: the arrow sequences in both forms, a lone ESC, Enter for `\r` and `\n`, Backspace for 127, and unknown bytes dropped
- [x] 8.3 Implement `src/platform/terminal_posix.cpp` and `terminal_windows.cpp` per §12, plus `src/engine/loop`. Temporarily point `heroes` at a demo that draws a moving box and the key name pressed; verify manually on macOS that q, Ctrl+C, and `kill <pid>` all leave the shell usable, and that shrinking below 80×24 shows the resize message. Report what was checked
  - Checked by the user on macOS, 2026-10-12, with the real game (the demo was replaced in 10.1): quitting, Ctrl+C, `kill <pid>`, and shrinking below 80×24 all behaved as required.
- [x] 8.4 Record full-redraw vs diff bytes per frame for the demo in `docs/benchmarks.md`
- [x] 8.5 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 9. Sprites

- [x] 9.1 Implement `src/engine/sprite.{hpp,cpp}` (parse + `drawSprite` + tints) per §12; verify `tests/sprite_test.cpp`: a transparent top pixel keeps the background; top-only, bottom-only, and both-opaque cells use the correct glyph and colors; odd height; errors carry line numbers for bad size, unknown palette key, and wrong row length
- [x] 9.2 Implement `tools/sprite_bake` (`sprite_bake in.png out.sprite [--max-colors 32]`: alpha < 128 is transparent; exact colors are kept if ≤ max, otherwise fail with a message to reduce the palette first); verify by baking `tests/fixtures/sample.png` (an 8×8 PNG checked in) and loading the result in a test
- [x] 9.3 Hand-write the 5 placeholder sprites listed in `ui.md`; verify they load through content validation
- [x] 9.4 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 10. Game Screens

Approved plan (2026-10-11): stop for approval after each of 10.0-10.5; commit locally at each stop and push once at the end of the chunk.

- [x] 10.0 Pre-chunk hardening from the chunk 1-9 review: cap content and save numbers (`kMaxStat`, `kMaxCurrency`) so hand-edited files fail cleanly instead of overflowing; Charisma refuses to pass the cap; reject duplicate recipe ingredients; `sprite_bake` checks size before converting; `heroes-sim` caps workers and range-checks bonuses; restore the terminal on `SIGABRT`. Plus the screen prerequisites: `upgradeCost`, `forgeChancePercent`, `charismaQuote` (screens preview costs without copying formulas), `TrainingRound::monsterMaxHp`, and centering the canvas in larger terminals; add the save-overwrite confirm rule to `ui.md`
- [x] 10.1 Implement `src/app/main.cpp`, `app`, `screen.hpp`, and `widgets` (Menu, TextInput, NumberInput, MessageBox, Typewriter, ConfirmBox, status panel) per §14 and `ui.md`; replace the chunk 8 demo; verify the Title screen renders and Quit works
- [x] 10.2 Implement Title → Intro → Create Hero → Hub, plus Save and Continue; verify manually: create a hero, save, quit, relaunch, Continue shows the same hero
- [x] 10.3 Implement Training Setup and Playback; verify manually: train at cave 1 for 5 rounds and confirm gold and exp in the status panel match the totals screen
- [x] 10.4 Implement the Merchant, Blacksmith, Guildmaster, and Bank screens; verify manually: buy a Health Potion, auction a Topaz, open the bank and deposit, attempt an unaffordable upgrade and see the red cost lines
- [x] 10.5 Implement Mission Select, Intro, `BattleView`, Battle, and Result per the `ui.md` animation table; verify manually by hand-editing the save JSON to level 20 (no cheat code is added), loading it, then playing Medusa once to defeat (the ported balance makes this the expected outcome) and once to Flee, and confirm the log, animations, and result screens match `ui.md`
- [x] 10.6 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 11. Final Verification

- [x] 11.1 On a fresh clone: `cmake --preset debug`, build, `ctest`; `cmake --preset asan`, build, `ctest`; verify all pass and CI is green on all four jobs
- [x] 11.2 Run a full playthrough: create → train a few rounds → shop → save → reload → (save edited to level 20) → Medusa → save → reload; verify the state matches and that `git status` shows no stray files
- [x] 11.3 Run `openspec validate build-core-engine --strict` and verify it passes; list any SPEC DRIFT found during the change for the user
- [x] 11.4 Complete chunk quality check against `docs/engineering/code-correctness.md`.
