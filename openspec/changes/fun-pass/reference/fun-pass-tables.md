# Fun pass: numbers and content

Boss stats marked *start* are starting points that the simulator tunes into the balance band (task 5.x). The tuned values replace them in this file when they ship.

## Battle actions

| action | effect |
|---|---|
| Item | one consumable from the battle stock, with its usual effect (Holy Water +100 HP, Holy Oil +10 Fervor, Viaticum full). Takes the turn. Removed from the bag when the battle ends. |
| Defend | Guard on the hero for 1 turn (it covers the enemy's turns until the hero's next turn ends) and +10 Fervor (capped) |
| Guard | incoming damage `/ 2` (integer), before Reflect |
| Charging | no effect of its own; a boss script sets it on itself one turn before a strong action and clears it when that action fires |

## Blessings (one point per level gained)

| choice | gain |
|---|---|
| Max HP | +30 (current HP +30) |
| Max Fervor | +10 (current Fervor +10) |
| ATK | +6 |
| DEF | +3 |

Version 1 and 2 saves load with `level − 1` points.

## Training pot

Each win adds `round gold × min(streak, 10) / 10` (integer) to the pot, where `streak` counts wins earlier in this session. A draw resets the streak. Leave banks the pot; a defeat forfeits it.

## Bosses

| id | name | mission (id, min level) | maxHp / atk / def / life (tuned in 4.3) | rewards | script behaviour |
|---|---|---|---|---|---|
| `avaritia` | Avaritia | `avaritia`, 15 | 3200 / 200 / 15 / 0 | 5000 EXP, 15000 gold, Gold Dust, Dark Pearl | Attacks; on a 1-in-4 roll says "Avaritia counts its coins..." and charges; the next turn strikes for 2.5×ATK with DEF applied, then clears Charging |
| `gula` | Gula | `gula`, 18 | 5400 / 410 / 25 / 0 | 7000 EXP, 20000 gold, Broken Compass, Holy Oil ×2 | Every attack heals Gula by half the damage dealt; on a 1-in-3 roll says "Gula opens its maw..." and charges, then devours for 2×ATK and heals all the damage dealt |
| `ira` | Ira | `ira`, 22 | 29000 / 325 / 40 / 0 | 10000 EXP, 30000 gold, Ember of Wrath | Attack power is `ATK × (100 + 100 × missing HP / max HP) / 100`; below half HP it charges on a 1-in-2 roll, then strikes for 2×(that power) |

Every boss hit varies by ±15% (`power × (85 + rng(31)) / 100`), so win rates aren't all-or-nothing (decided 2026-10-13, task 4.3).

Mission names, locations and story lines:
- `avaritia`: unchanged (Act I: "Avaritia", THE COUNTING HOUSE).
- `gula`: Act I: "Gula", THE EMPTY FEAST. "The banquet hall has eaten the city's bread for a year." / "It is still hungry."
- `ira`: Act I: "Ira", THE BURNING GATE. "Wrath holds the east gate, and it burns hotter the more it bleeds." / "Do not let it rage."

Sprites: `gula.sprite` and `ira.sprite` at 32×28 compact, and both large looks at 56×52, made with the Draw Things pipeline (prompts and seeds recorded in `tools/art/README.md`).

## Intended builds for the balance band

| boss | weapon | armor | skills | blessings (spread evenly over HP/Fervor/ATK/DEF) | items |
|---|---|---|---|---|---|
| Avaritia | `iron_sword:5` | `steel_shield:5` | Guardian Angel, Righteous Strike | 14 | 2 Holy Water |
| Gula | `fire_claymore:1` | `steel_shield:5` | Guardian Angel, Righteous Strike, Smite | 17 | 3 Holy Water, 1 Holy Oil |
| Ira | `fire_claymore:3` | `diamond_plate:1` | Guardian Angel, Righteous Strike, Smite, Sanctuary | 21 | 3 Holy Water, 2 Holy Oil |

The simulator gains `--blessings N` (spread evenly in the listed order, with remainders going to HP first) and `--items id:count,...` to express these.

## New skills and item

| id | name | Latin | Fervor | level | gold | key item | effect (script) |
|---|---|---|---|---|---|---|---|
| `smite` | Smite | Percute | 20 | 12 | 6000 | Martyr's Relic (`gold_leaf`) | `ctx.damage(atk × (3 if target charging else 1), true)` |
| `sanctuary` | Sanctuary | Sanctuarium | 30 | 16 | 12000 | Tears of Penance (`frozen_tears`) | Guard on self for 2 turns, heal 20% of max HP |
| `judgment` | Judgment | Iudicium | 45 | 22 | 25000 | Ember of Wrath (`ember_of_wrath`, new) | `3×ATK` ignoring DEF if the target is below 25% HP, else `ATK` with DEF |

New item `ember_of_wrath`: Ember of Wrath, "Favilla Irae. A coal that never cools.", skill key, price 0, auction value 4000, drop rate 0 (Ira's reward only).

Descriptions follow the reskin style ("Percute. Strikes harder at an enemy gathering its strength.", "Sanctuarium. A moment of shelter and healing.", "Iudicium. Ends what is already failing.").

## Hub "Next:" wording

| situation | line |
|---|---|
| hero at 0 HP | Next: visit the Prior to be revived |
| unspent points | Next: spend N blessing(s) at the Prior |
| below the next Act I mission's level | Next: train to Lv L for MISSION NAME |
| estimate below 50% | Next: BOSS (Lv L) — your odds ≈P%; upgrade gear, learn skills, or train |
| estimate 50% or more | Next: BOSS (Lv L) — your odds ≈P%; you're ready |
| Act I done, bonus left | Next: the Gorgon awaits (Lv 20), if you dare |
| everything done | Act I is complete. |
