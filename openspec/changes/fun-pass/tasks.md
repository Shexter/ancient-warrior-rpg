# Tasks

## 1. Battle decisions

- [x] 1.1 Add `Guard` and `Charging` statuses (Guard halves damage before Reflect), `UseItem` and `Defend` actions, the battle item stock, `ItemUsed`, and `itemsUsed()`; `finishMission` removes used items on every outcome; verify battle tests for the Holy Water, item-not-in-stock, Defend-against-a-charge and Guard-timing scenarios, and that the Medusa golden tests pass with `battle_golden_data.hpp` unchanged
- [x] 1.2 Move the smart policy into `game/autoplay` (defend on Charging, heal with Angel Wings or the best item below 30%, strongest damage skill, attack); `heroes-sim` uses it and gains `--items id:count,...` (`--blessings N` moves to 2.1, which defines the gains); verify sim tests (thread independence and the policy scenarios) pass
- [x] 1.3 Battle screen: five actions with shortcuts 1-5, an Item overlay with counts, `[GUARD n]`/`[CHARGE]` tags, log lines for Defend and items, in compact and large layouts; verify screen tests for each, and that the compact battle fingerprint test still passes (it covers the battle view, not the action row, so it needed no update)
- [x] 1.4 Update SimpleDocs (`hero-and-battle.md`, `app.md`, `simulator.md`); verify they describe the new actions and statuses
- [x] 1.5 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 2. Blessings

- [x] 2.1 Add blessing points to the hero (one per level gained in `grantExp`), the all-or-nothing `blessing(state, stat)` action, save version 3 (`level − 1` points for v1/v2 saves), and `heroes-sim --blessings N` (spread evenly, remainders to HP first); verify hero, town and save tests for the multi-level, spend, no-points, cap and pre-blessing-save scenarios
- [x] 2.2 Prior "Blessings" menu (stat choice with the gain shown, points left) and level-up messages that mention new points (battle result and training totals); verify screen tests
- [x] 2.3 Update SimpleDocs (`hero-and-battle.md`, `town-and-training.md`, `save.md`, `app.md`); verify against the screens
- [x] 2.4 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 3. Push-your-luck training

- [ ] 3.1 Add `TrainingSession` (one RNG fork per session, `next()` per round, streak and pot, `leave()` banks it, defeat forfeits it) sharing the round function with `runTraining`; verify tests for the banking and death scenarios, determinism (same seed, same choices, same results), and that the existing `runTraining` tests pass unchanged
- [ ] 3.2 Add win-chance estimation for caves (`game/estimate`, copies only, deterministic) and measure its cost for all 30 caves; verify tests that the state and RNG are untouched and that estimates repeat
- [ ] 3.3 Training screens: a cave list with estimates, round playback as today, then Continue / Leave with the streak and pot shown, and totals; verify screen tests for continue, leave and a defeat forfeiting the pot
- [ ] 3.4 Update SimpleDocs (`town-and-training.md`, `app.md`); verify against the screens
- [ ] 3.5 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 4. Act I bosses

- [ ] 4.1 Generate Gula and Ira sprites with Draw Things (compact 32×28, large 56×52 in both looks) and record prompts and seeds; show the user a preview before committing
- [ ] 4.2 Add `avaritia.lua`, `gula.lua` and `ira.lua` (telegraphs via Charging) with the enemies and missions from the reference table, in Act I order; verify script tests for each telegraph, Gula's drain and Ira's scaling, and that Medusa's goldens are unchanged
- [ ] 4.3 Tune each boss with `heroes-sim` to the band (smart 50–80% and attack at least 30 points lower for every path with its intended build; smart below 5% with starting gear), record the runs in `docs/benchmarks.md`, update the tuned stats in the reference table, and pin all three with band tests; if a path can't fit, stop and report
- [ ] 4.4 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 5. Act I skills

- [ ] 5.1 Add Smite, Sanctuary and Judgment (scripts) and the Ember of Wrath item per the reference table; verify script tests for the Smite-a-charging-boss scenario, Sanctuary's guard and heal, and Judgment's threshold, and that the skill overlay and Learn Skill list fit with nine skills (example mod included)
- [ ] 5.2 Re-run the band tests from 4.3 with the new skills in the builds; re-tune if needed and record
- [ ] 5.3 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 6. What next

- [ ] 6.1 Add mission win-chance estimates and `nextGoal(state, content, scripts)` per the reference wording; verify tests for every row of the wording table and that estimating leaves the state untouched; measure the Hub cost
- [ ] 6.2 Show the "Next:" line on the Hub (compact and large), refreshed when the state changes, never per frame; verify screen tests
- [ ] 6.3 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 7. Final verification

- [ ] 7.1 Play a fresh game on the real binary from creation to Ira, using only in-game guidance (no save edits), and record the session (time, levels, choices that mattered) in `docs/benchmarks.md`; report anything still unfun
- [ ] 7.2 Verify CI is green on all jobs and `openspec validate fun-pass --strict` passes; list any spec drift
- [ ] 7.3 Complete chunk quality check against `docs/engineering/code-correctness.md`.
