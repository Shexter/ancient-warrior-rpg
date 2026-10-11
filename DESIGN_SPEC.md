# Ancient Warrior RPG (Heroes RPG) — Design Spec from the original

> **Superseded for numbers and rules.** This was a first-pass summary. A line-by-line read of the original later corrected several points, and the authoritative values are in `openspec/changes/build-core-engine/reference/content-tables.md`. Known errors below:
> - "Restore (revive/heal, 100 gold)" is wrong. Restoration uses consumables. Revive costs up to 100 gold and sets HP to 100.
> - "buy stat points (100 gold per point)" is incomplete. Charisma spends EXP (50 per point) **and** gold, and requires level 10.
> - EXP is a spendable currency separate from level progress.
> - Upgrade material: weapons use Fire Dust, armor uses Turtle Shell.
> - The original price array was misaligned, so prices were decided anew.

Source: `~/.../My Drive/C++ Code/heroes RPG` (main, functions, npc, mission, training, login). This file lists the mechanics worth carrying over, plus things the original got wrong that the rewrite should fix.

## Core loop
Sign in/up → create hero (first run) or load save → main menu: **Training, Mission, NPC, Save, Exit**. Hero must wear a weapon AND armor to enter Training or Mission. Dead hero (HP 0) is locked out of Training, Missions, and shops until restored by the Guildmaster.

## Hero
- Paths (1 Fighter, 2 Hunter, 3 Rogue). Starting stats (maxMP / maxHP / ATK / DEF / LIFE):
  - Fighter 50 / 350 / 50 / 5 / 0
  - Hunter 100 / 200 / 100 / 5 / 10
  - Rogue 200 / 100 / 175 / 25 / 10
- `LIFE` = flat HP regained per turn/attack (lifesteal-ish).
- Name max 8 chars. Start: Lv1, 20,000 gold, Iron Sword (11) + Steel Shield (21).
- Level-up: `TNL = level * 100 * path`; one EXP gain can cascade several levels.
- Paths are an integer multiplier everywhere (TNL, stat-buy scaling, skill cost). Rewrite: make path a data-driven struct with explicit multipliers, not a magic int.

## Combat
Effective stat = hero base + equipment attr * equipment upgrade level. Damage = `max(0, ATK - target DEF)`.

**Training** (auto-battle, no input): choose cave level 1–30 and rounds.
- Monster: HP `100*L`, DMG `(rand 15–19)*2*L`, DEF `(rand 5–9)*L`; reward gold `(10–19)*10*L`, EXP `100*L`.
- Hero hits first each tick, monster hits back, hero regens LIFE. Death costs the EXP gained for that fight and leaves HP 0.
- Drop roll per win: pick random drop-table entry, succeed if `rand%100 < dropRate + caveLvl`.

**Mission / boss** (turn-based, interactive): Attack / Skill / Quit.
- Mission unlocks at Lv20; each mission is one-time.
- M1 "The Gorgon" (Medusa): HP 3000, DMG 250, DEF 20, LIFE 10. Reward 10,000 EXP, 25,000 gold, Thor's Hammer. Boss turn: 50% normal hit + regen, 50% hit with 60% chance of a double turn.
- Death in a boss fight destroys all equipment.
- M2 Lake of Lerna (Hydras, 3 random tiers, scales with missions cleared, 5 drop rolls) and M3 "The Twin" (two bosses, pay-for-hint dialogue) are commented-out sketches, "Coming Soon". The scaling formulas are in `mission.cpp` if wanted.

## Skills
Five skills, only usable in boss fights, cost MP, bought from the Guildmaster (needs level `5*n`, a key item `31+n`, and gold).

| # | Name | MP | Intended effect |
|---|---|---|---|
| 1 | Angel Wings | 10 | Full heal |
| 2 | Meteor Fist | 25 | 0.8 × current HP as damage |
| 3 | Golden Bell | 25 | Damage + heal half of it + reflect |
| 4 | Frozen Throne | 40 | Damage, multi-turn effect |
| 5 | Supernova | 50 | Never implemented |

Original intent for #4: stun (triple turn) + lower boss defense. #5: halve boss HP, 5× damage, zero armor.

## Items (20, ID-coded by category)
`1x` weapons, `2x` armor, `3x` skill-key items, `4x` crafting mats, `9x` consumables. Each has ATK/DEF/LIFE attrs, a gold cost (0 = untradeable), and a drop rate.
- Weapons: Iron Sword (50 ATK), Fire Claymore (crafted), Thor's Hammer (mission reward).
- Armor: Steel Shield, Diamond Plate, Peace Maker (crafted).
- Consumables: Health Potion, Mana Potion, Eternal Youth (HP+MP restore).
- 10-slot inventory, stackable; same again for the Bank.
- Rewrite: load from JSON. This is the single biggest win over the parallel arrays.

## NPCs
- **Merchant**: buy/sell by ID, untradeable items blocked, plus an "auction" that sells at a random result.
- **Blacksmith**: swap equipment; **upgrade** (cost per level L: 1 Fire Dust-type item × L, `1000*L` EXP, `10,000*L` gold, then equipment level +1, which multiplies its attrs); **forge** from recipes: Iron Sword + Phoenix Feather + Fire Dust → Fire Claymore, Steel Shield + Black Pearl + Broken Compass + Gold Dust → Peace Maker. Success chance depends on a forge level (starts 9, max 10).
- **Guildmaster**: restore (revive/heal, 100 gold), buy stat points (100 gold per point, gain scaled by `10*path/level`), learn skills, rename hero (50,000 gold).
- **Bank**: 10,000 gold to open, extra 10-slot storage.

## Persistence
Plain-text `game0..6.dat`, one value per line, plus a Caesar-style "encrypted" login. Rewrite: one JSON save file; skip the fake crypto (hash passwords if accounts are kept at all, or drop accounts entirely, since it's a single-player game).

## Bugs / quirks to NOT carry over
- `showStr`/`showNum` use `=` in `if(LR='L')` (assignment), so alignment is always one branch.
- Sleep-based pacing everywhere blocks input; also `system("cls")`/`pause`, `PlaySound` (Windows-only).
- Upgrade code checks item ID 42 (Turtle Shell) while its text says Flaming Dust (41). Pick one.
- Skill 4 is hard-wired to `turn` counter hacks; skill 5 missing.
- Stat-buy formula `points*10*path/level` shrinks gains as level rises. Decide whether intended.
- Debug backdoor menu entries 7/8/9 (add level/EXP/gold) and a `vip()` passcode. Fold into a dev console behind a flag.
- Rand-loop `rndDrop` can spin forever if the table is all zeros; use a weighted pick.
- Missions 2/3 duplicate ~150 lines; use one `Encounter` driven by data.

## Suggested rewrite mapping
| Original | Rewrite |
|---|---|
| Parallel arrays | `ItemDef` loaded from `items.json` |
| Giant param lists | `Hero`, `Stats`, `Inventory` types |
| mission1/2/3 copies | one `Battle` class + `EnemyDef` data + skill/effect list |
| `turn` int hacks | status effects with durations (stun, reflect, def-down) |
| Sleep/cout/cin | game logic returns events; renderer/UI consumes them |
| Training auto-fight | headless `simulate(hero, enemy)`, ready for the 100K-battle benchmark |
