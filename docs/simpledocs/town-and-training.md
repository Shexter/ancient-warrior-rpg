# Training and town (`training.*`, `town.*`)

**Rule:** every function checks all conditions first and mutates after. On an error the `GameState` is bit-identical to before (tests compare with `==`). Errors are `TownError`; `describe()` gives the player-facing sentence.

## Training
`runTraining(state, content, cave 1..30, rounds 1..100)`: dead or unequipped hero is refused before range checks. One `fork()` for the whole session. Each round rolls a monster (atk, def, gold in that order), then hero and monster trade hits (hero first, then the hero heals `life`), capped at 1000 exchanges (Draw). Victory pays gold and EXP and rolls a drop (`grantMonsterRewards`, shared with the Overworld); Defeat sets HP to 0, deducts that round's EXP (not below 0, `loseToMonster`) and stops. Hits use `defendedDamage` (`battle.hpp`), the game's one damage rule. HP carries between rounds. Each round also reports `monsterMaxHp` (for the playback HP bar). Events are `Damaged`/`Healed` only. Drops lost to a full bag are reported, not hidden. The simulator and older tests use it; the game uses `TrainingSession`.

`TrainingSession::start(state, content, cave)` makes the same checks (no round count) and forks the RNG once; `next()` plays one round with exactly the same round function, so a session's rounds match `runTraining`'s for the same seed. Each win adds `potGained = round gold × min(streak, 10) / 10` to `pot()` (streak = wins earlier in the session); a draw resets `streak()`; a defeat empties the pot, resets the streak and ends the session (`over()`). `leave()` adds the pot to the hero's gold, ends the session and returns what it banked (0 after a defeat). The pot is pending until then, so the hero's gold only counts round rewards.

`caveWinChance(state, content, cave)` (`estimate.*`) is the win chance, in whole percent, of the next round at that cave with the hero's current HP: 200 fights on copies, each monster rolled from `Rng::stream(seed ^ (0xCA7E… + cave), i)`. It is deterministic for a state and never touches the state or its RNG. Training rounds have no player choices, so no policy is involved. All 30 caves take about 7 ms in a debug build and 0.1 ms in release.

`missionWinChance(state, content, missionId, scripts)` (same files) plays 200 mission battles with the `smart` policy, the learned skills and the bag's consumables, on streams from `seed ^ salt(mission id)`; one estimate takes about 8 ms in a debug build. `nextGoal(state, content, scripts)` builds the Hub's "Next:" line: revive, spend blessings, train to the first open Act I mission (a name starting "Act I:"), its odds (below 50% suggests getting stronger), then the bonus mission, then "Act I is complete." The Hub shows it (see `app.md`).

## Merchant (refuses a dead hero)
- `buy`: 1–99 units at the item price. `sell`: full price. Items with price 0 are not tradeable.
- `auction`: one unit, fixed auction value, 1-in-10 no buyer. Otherwise up to `r` bid rounds each with a 1-in-3 chance to stop, +10% of value per bid, 10% fee. Payout is 450–855 for a 500-value item. Forks the RNG even when nothing sells.

## Blacksmith (refuses a dead hero)
- `equip`: old item returns to the bag at upgrade 1; refused if the bag cannot take it, if the item is already equipped, or for the wrong slot.
- `upgrade` at level L (to 10): consumes L materials, 1000·L EXP, 10000·L gold (`upgradeCost(L)`, also used by the screens). The limits the screens print (`kMaxUpgradeLevel`, `kBankCost`, `kRenameCost`, `kCharismaMinLevel`, `kLearnMinLevel`) are named in `town.hpp` so the text can't drift from the rules.
- `forge`: ingredients must be in the bag (equipped items don't count) with room for the result. Ingredients are consumed even on failure; success chance is forge level × 10% (`forgeChancePercent`), and success raises forge level to at most 10.

## Guildmaster / bank (allowed when dead)
- `revive` (returns `bool`; spec said `expected`): hp 0 → `min(100, maxHp)` for `min(100, gold)` gold, so a broke hero is never stuck.
- `useConsumable`, `learnSkill` (level ≥ max(5, skill level); consumes the key item and gold), `rename` (50,000 gold).
- `blessing(state, stat)`: spends one blessing point for +30 max HP, +10 max MP, +6 ATK or +3 DEF (`blessingGain(stat)`); a max HP/MP blessing raises current HP/MP by the same amount. Refused with nothing changed without a point (`NoBlessings`) or if the stat would pass 1,000,000 (`InvalidQuantity`). Works on a fallen hero too.
- `buyCharisma`: level ≥ 10; **spends all the EXP entered**, but points = EXP/50, cost 100 gold per point, gain = points·10·multiplier/level. Refused with nothing spent if points < 1, gain < 1, or the stat would pass 1,000,000. `charismaQuote(hero, content, exp)` returns the points, gold cost and gain without changing anything, for previews. Does not raise current HP/MP.
- Bank: 10,000 gold once, 10 slots, `deposit`/`withdraw` move items atomically.

## Missions
- `startMission` builds a `Battle` (forks the RNG once). Refused if dead, unequipped, below the minimum level (`MissionLocked`) or already completed and not repeatable.
- `finishMission` (precondition: battle over) copies HP/MP back every time. Victory: EXP, gold, reward items (a full bag loses them and reports it), marks completed. Defeat: HP 0, weapon and armor destroyed (bag kept). Fled/Draw: nothing else changes.
