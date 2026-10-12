# Hero, inventory, battle

## Hero (`hero.*`, `game_state.hpp`, `inventory.*`)
- `newGame` starts at level 1, 20,000 gold, full HP/MP, `iron_sword` and `steel_shield` at upgrade 1, forge level 9, empty bag, no bank.
- `exp` is a **spendable currency**; `grantExp` adds to it and also advances level progress (`expToNext`). Leveling never raises stats. Stats change only through equipment, upgrades and Charisma.
- `equipmentBonus` = sum of item bonus × upgrade level over weapon and armor.
- `Inventory`: 10 slots, identical items stack without limit; `canAdd` is true for an existing stack or a free slot.
- `validateHeroName` trims spaces and accepts 1–8 characters in 0x20–0x7E.
- All state types have `==`.

## Battle (`battle.*`, `events.hpp`)
- Actions in, events out: `act(Attack | CastSkill | Flee)` returns the events or an `ActionError` with no state change. No I/O.
- Turn order, damage, statuses (`stun`, `defense_down`, `reflect`, with the `fresh` flag), Medusa's enemy pattern and the 1000-turn cap follow `interfaces.md` §7 exactly. Damage is not clamped to HP in the formula, but HP never drops below 0.
- Verified against the four golden event sequences in `tests/battle_golden_test.cpp`, which must never be edited to make a test pass.
- **Player actions:** `Attack`, `CastSkill`, `UseItem{id}`, `Defend`, `Flee`.
  - **Items:** a battle starts with an `ItemStock` (consumable id → count). `startMission` copies the bag's consumables into it. `UseItem` spends one and emits `ItemUsed`, then the item's usual effect: `Healed`, and the new `MpRestored{target, amount, mpAfter}` when MP goes up. Using an item takes the turn. `items()` is what's left and `itemsUsed()` what was spent; `finishMission` removes the spent ones from the bag on every outcome. An item not in the stock (or not a consumable) is `ActionError::ItemNotInStock`, with nothing changed.
  - **Defend** applies Guard to the hero for 1 turn and restores 10 MP. Guard is "fresh" on the hero's own turn, so it covers the enemy turns that follow and expires at the end of the hero's next turn.
- **Statuses** now also include **Guard** (incoming damage halved, integer, before Reflect splits it) and **Charging** (no engine effect; a boss script sets it on itself a turn before a strong action, so the player can see it coming).
- **Scripts** (see `scripting.md`): `Battle` takes an optional `ScriptHost*`. An enemy with a `script` runs its `take_turn` instead of the native pattern. A skill with a `script` spends MP, emits `SkillCast`, then runs `cast`. Two events are new: `ScriptSaid{side, text}` and `ScriptError{script, message}`. One `ActionError` is new: `ScriptFailed`. A failed script leaves no trace: enemies fall back to a basic attack, and skills roll the action back.
- Death checks (`Defeated` and `BattleEnded`, hero first) are one function shared by Medusa's pattern, the basic attack and every script operation. The only native enemy behavior left is `basic` (attack every turn), which is also the fallback for a failed script. Medusa's pattern is now `data/scripts/medusa.lua`, and its golden sequences are unchanged.
