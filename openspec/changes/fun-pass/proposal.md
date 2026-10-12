# Proposal

## Why

The game works but isn't fun yet (assessment of 2026-10-12, agreed with the user):
1. **Bosses are decided by gear.** Medusa is 0% or 100% depending on one gear step, and the player's choices in battle barely matter.
2. **Potions can't be used in battle**, so a losing fight has no comeback.
3. **Training is typing two numbers** and watching a replay.
4. **Nothing says what to do next.**
5. **Levels feel hollow**, because they change no stats.
6. **The content is thin:** three missions and five skills.

The user chose the direction for each (2026-10-12): blessing points, battle decisions with the damage formula kept, push-your-luck training, and two sin bosses with three scripted skills.

## What Changes

- **Battle decisions.** The action row becomes Attack / Skill / Item / Defend / Flee.
  - **Defend** halves damage taken until your next turn and restores some Fervor.
  - **Item** uses a consumable from your bag mid-battle; it's removed from the bag when the battle ends, whatever the outcome.
  - **Telegraphs.** Bosses can "charge", which is a visible status, and hit hard on their next turn, so Defend, healing and timing matter.
  - The damage formula is unchanged, and so are Medusa and her golden battles. She stays the brutal bonus boss.
- **Blessings.** Each level gained gives 1 blessing point, spent at the Prior on +Max HP, +Max Fervor, +ATK or +DEF. **BREAKING (rule):** levels no longer leave stats untouched, though only through the player's choice. Saves move to version 3. An older save's hero gets `level − 1` points, so nobody is left behind.
- **Push-your-luck training.**
  - Pick a cave from a list showing your estimated win chance for each, then fight one round at a time and choose **Continue** or **Leave** after each.
  - Consecutive wins build a gold **pot**: Leave banks it, a defeat loses it.
  - Per-round rewards are unchanged. **BREAKING (rule):** there's no round count any more.
- **What next.** The Hub shows one "Next:" line, for example "Next: Avaritia (Lv 15) — your odds ≈35%; upgrade gear or train". Its odds come from the same estimator as training.
- **Content.**
  - Avaritia gets its own script with a telegraphed greed strike.
  - Two new Act I sin bosses, **Gula** (heals from the damage it deals) and **Ira** (hits harder as it weakens), each with a mission, sprites in every size and look, and a reward.
  - Three new scripted skills: **Smite** (triple damage on a charging enemy), **Sanctuary** (guard and heal), and **Judgment** (a finisher). One new key item.
  - Mission order: Avaritia (15), Gula (18), Ira (22), then the Medusa bonus (20).
- **Balance with choices.** Each Act I boss is tuned so the `smart` policy (which defends, heals and uses items) wins 50–80% with its intended build, while attack-only wins at least 30 points less. The choices are proven to matter.

## Capabilities

### New Capabilities
- `player-guidance`: estimated win chances and the Hub's "Next:" goal.

### Modified Capabilities
- `combat`: player actions gain Item and Defend; a Guard status and a Charging status.
- `hero-progression`: leveling grants blessing points; training becomes round-by-round push-your-luck.
- `town-services`: the Prior spends blessing points.
- `save-system`: save version 3 records blessing points.
- `balance-simulator`: the `smart` policy defends, heals and uses items; the Act I band covers three bosses and requires choices to matter.
- `game-content`: the Act I mission list (Avaritia with its own pattern, Gula, Ira, then Medusa) and the new skills and item.

## Impact

- **Game logic:** `battle` (actions, statuses, item stock), `hero` (blessings), `training` (sessions), `town` (blessings, consumables at finish), `save` (v3), new `autoplay` (the smart policy, moved from `tools/sim` so the estimator and the simulator share it).
- **App:** the battle action row and item overlay, the Prior's Blessings screen, the new training screens, the Hub's Next line.
- **Data/art:** `scripts/avaritia.lua`, `gula.lua`, `ira.lua`, three skill scripts, two enemies, two missions, a key item, and Gula and Ira sprites (compact plus large in both looks).
- **Tests:** Medusa's golden vectors and `battle_golden_data.hpp` stay untouched. The Avaritia band tests are re-pinned to the new band. Training, save and screen tests follow the rule changes.
- **Depends on:** `add-lua-scripting` being archived first (the bosses and skills are scripts).
