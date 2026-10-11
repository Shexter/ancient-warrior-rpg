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
