# Design

## Context

See proposal.md for why. Relevant current state, after `add-lua-scripting`:
- **Battles:** `Battle::act(Attack | CastSkill | Flee)` emits events, and enemy turns and skills can be Lua scripts.
- **Statuses** are a small enum (`stun`, `defense_down`, `reflect`) with the "fresh" countdown rule.
- **Training** is `runTraining(state, content, cave, rounds)`, which applies everything at once with one RNG fork.
- **Leveling** never changes stats.
- **The smart policy** lives in `tools/sim`.
- **Saves** are version 2 (mods).
- **Golden vectors:** Medusa's pin the damage formula and her pattern (`battle_golden_data.hpp`). This change must leave them byte-identical.

## Goals / Non-Goals

**Goals:**
- Every Act I boss fight turns on decisions: defend at the telegraph, heal or use items, time the finisher.
- Progress is legible in-game: odds on caves and missions, and a single next step.
- Levels feel like something, while growth still comes from the player's choices (world-bible §1).

**Non-Goals:**
- No change to the damage formula, the drop roll, Medusa, or her script.
- No randomness added to damage.
- No new UI screens beyond those named (no quest log, no codex).
- No Avaritia gold theft (battles don't own gold; still deferred).

## Decisions

**1. The smart policy moves into the game (`game/autoplay`).**
- Estimates and the simulator must agree, and one policy in one place guarantees it.
- `tools/sim` calls it, and the Hub and training estimates call it.
- Alternative: duplicate a simpler policy for estimates. Rejected, because the two would drift and the odds would lie.

**2. Estimates are simulations on copies, never on live state.**
- The estimate for a fight is the share of N = 200 fights won with the smart policy.
- Fights use `Rng::stream(state.seed ^ salt, i)`, where the salt is derived from the target (the cave number, or a hash of the mission ID). The live `GameState::rng` is never touched, so it's deterministic for a given state.
- Mission estimates need a `ScriptHost`. The Hub uses the session's.
- **Cost:** about 30 caves × 200 rounds, plus 3–4 missions × 200 battles, measured in the task. It's computed when the Hub or cave list opens and when the state changes, never every frame.
- Alternative: a closed-form power ratio. Rejected, because statuses, scripts and telegraphs make it wrong exactly where it matters.

**3. Guard and Charging are statuses.** They reuse the countdown, events, tags and save-free lifetime that statuses already have.
- Guard halves damage in `hit()` before Reflect.
- Charging has no engine effect; scripts read it with `ctx.self.has("charging")` and set it with `ctx.apply_status`.
- The enum gains `Guard` and `Charging`; the battle view shows `[GUARD n]` and `[CHARGE]`.
- Alternative: a separate "telegraph" event channel. Rejected: the status is already visible, already testable, and policies can read it.

**4. Battle items are a stock copied in at start.**
- `Battle` gets the hero's consumable counts. `act(UseItem{id})` applies the effect through the existing heal logic and emits `ItemUsed{id}`.
- `Battle::itemsUsed()` reports the total, and `finishMission` removes those items from the bag on every outcome.
- Alternative: give `Battle` the bag. Rejected, because battles stay pure functions of their inputs, which keeps goldens and estimates simple.

**5. Blessings are a hero counter plus a Prior action.**
- `grantExp` adds one point per level gained.
- `blessing(state, stat)` is all-or-nothing like other town actions (spec: town-services).
- Saves become version 3 with `blessings`; versions 1 and 2 load with `level − 1`.
- `heroes-sim --blessings N` spreads points evenly so the bands describe a real player.

**6. Training becomes a session object.**
- `TrainingSession(state, content, cave)` forks the RNG once (the rule is unchanged).
- `next()` fights one round and applies its results as today. `leave()` banks the pot.
- The pot is pending until Leave, so a defeat forfeits only it.
- The old `runTraining` stays for the simulator and existing tests, with no pot and its numbers unchanged, both implemented on one shared round function.
- Alternative: drop `runTraining`. Rejected, because its tests pin the round rules the session reuses.

**7. Bosses and skills are scripts.**
- `avaritia.lua`, `gula.lua` and `ira.lua` use only the existing ctx API plus the new statuses. The three skills are scripts too.
- Medusa's `medusa.lua` is untouched, and Avaritia stops sharing it.
- The band is re-pinned by the `AvaritiaBalance`-style tests for all three bosses, with two conditions:
  - smart wins 50–80%;
  - attack-only wins at least 30 points less, so choices measurably matter.

**8. Action row and overlays.**
- The action row has five entries (Attack, Skill, Item, Defend, Flee); shortcuts 1–5 and ←/→ still work.
- Item opens an overlay like Skill, listing the stock with counts.
- Both layouts fit: the compact row needs about 52 columns.

## Risks / Trade-offs

- [Balance may not reach "smart 50–80% and attack ≥30 points lower" for every path] → Tune each boss's stats and telegraph cadence with the simulator. If a path can't fit, report it with the measured numbers rather than widen the band silently (the same rule Avaritia followed).
- [Estimates cost time on the Hub] → Measure. If above about 30 ms, lower N or cache per state. Never estimate per frame.
- [Blessings retroactively strengthen old saves] → That's intended. The bands are measured with the matching blessing counts, so older heroes land where new ones would.
- [Two statuses widen the battle view's tag row] → Tags are already clipped to their column; check both layouts.
- [A rule change ripples through existing tests (leveling scenario, training)] → Only tests asserting the changed rules are updated; the golden vectors and the Medusa tests are untouched.

## Migration Plan

Saves move from version 2 to 3: older saves load with `level − 1` blessing points and are written as version 3 on the next save. Mods keep working; mod skills and enemies can use Guard and Charging through the existing ctx.
