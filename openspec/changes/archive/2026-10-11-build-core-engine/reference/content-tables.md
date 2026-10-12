# Content Tables (authoritative)

Every number the shipped game uses. `data/*.json` must reproduce this file exactly. Where the original source (`heroes RPG/`) was buggy or inconsistent, the decision made here is marked **[decided]** with the reason, and that decision overrides `DESIGN_SPEC.md`.

## Hero paths

| id | name | multiplier | maxHp | maxMp | atk | def | life |
|---|---|---|---|---|---|---|---|
| `fighter` | Fighter | 1 | 350 | 50 | 50 | 5 | 0 |
| `hunter` | Hunter | 2 | 200 | 100 | 100 | 5 | 10 |
| `rogue` | Rogue | 3 | 100 | 200 | 175 | 25 | 10 |

Flavor text (shown at path selection): Fighter "blessed with the Strongest Fist", Hunter "a Body Strong as rock", Rogue "a Balance Mind".

Note: leveling does **not** raise stats. Stats grow only through equipment, upgrades, and Charisma (guildmaster). This matches the original.

## Starting state

- Level 1. `expToNext = 100 × multiplier`. Spendable `exp = 0`. `gold = 20000`.
- HP and MP full.
- Weapon `iron_sword` at upgrade level 1. Armor `steel_shield` at upgrade level 1.
- Empty 10-slot bag. No bank account. `forgeLevel = 9`. No skills. No missions completed.

## Items

Columns: kind W=weapon, A=armor, K=skill key, M=material, C=consumable. `price` 0 = not sold or bought at the merchant. `auction` 0 = cannot be auctioned. `drop` = drop rate %, 0 = never dropped.

| id | name | kind | atk | def | life | price | auction | drop | description |
|---|---|---|---|---|---|---|---|---|---|
| `iron_sword` | Iron Sword | W | 50 | 0 | 0 | 1000 | 0 | 30 | Sharp and Light Sword |
| `fire_claymore` | Fire Claymore | W | 1000 | 100 | 100 | 10000 | 0 | 0 | Fiery Great Weapon |
| `thors_hammer` | Thor's Hammer | W | 700 | 400 | 0 | 0 | 30000 | 0 | Hammer of Thunder god |
| `steel_shield` | Steel Shield | A | 0 | 20 | 0 | 1000 | 0 | 25 | Thick Shield |
| `diamond_plate` | Diamond Plate | A | 0 | 200 | 100 | 5000 | 0 | 0 | Shiny and Pure Plate |
| `peace_maker` | Peace Maker | A | 1000 | 1000 | 100 | 20000 | 0 | 0 | Incredible Destructive Artifact |
| `topaz` | Topaz | K | 0 | 0 | 0 | 0 | 500 | 40 | Hard Solid Gemstone |
| `gold_leaf` | Gold Leaf | K | 0 | 0 | 0 | 0 | 1500 | 15 | Leaf from OtherWorld |
| `frozen_tears` | Frozen Tears | K | 0 | 0 | 0 | 0 | 1500 | 15 | Tears of Snow Queen |
| `broken_staff` | Broken Staff | K | 0 | 0 | 0 | 0 | 3000 | 7 | Piece of Magical Wood |
| `pure_ice` | Pure Ice | K | 0 | 0 | 0 | 0 | 3000 | 7 | Frozen white Rock |
| `fire_dust` | Fire Dust | M | 0 | 0 | 0 | 0 | 1000 | 10 | Flaming Dust |
| `turtle_shell` | Turtle Shell | M | 0 | 0 | 0 | 0 | 2000 | 5 | Ancient sea creature |
| `gold_dust` | Gold Dust | M | 0 | 0 | 0 | 0 | 1000 | 10 | Sprinkle of Magic |
| `black_pearl` | Black Pearl | M | 0 | 0 | 0 | 0 | 2000 | 5 | Dark Sphere of the Deep |
| `broken_compass` | Broken Compass | M | 0 | 0 | 0 | 0 | 2500 | 4 | Magical Compass |
| `phoenix_feather` | Phoenix Feather | M | 0 | 0 | 0 | 0 | 3000 | 3 | Ancient Fiery Bird |
| `health_potion` | Health Potion | C | – | – | – | 1000 | 0 | 50 | Restore Health |
| `mana_potion` | Mana Potion | C | – | – | – | 1000 | 0 | 50 | Restore Mana |
| `eternal_youth` | Eternal Youth | C | – | – | – | 0 | 0 | 25 | Rebirth your Soul |

Consumable effects: `health_potion` restores 100 HP. `mana_potion` restores 10 MP. `eternal_youth` fully restores HP and MP. All are capped at max.

Upgrade material: weapons use `fire_dust`, armor uses `turtle_shell`.

**[decided] Prices.** The original `itemCost[]` array was misaligned with `itemID[]`, which made Health Potion unpurchasable and gave Steel Shield no price. The prices above follow the evident intent.

**[decided] Auction values.** The original let the player type any minimum price, which allowed unlimited gold. Each item now has a fixed auction value. The auction values are new numbers, tunable through the simulator later.

## Skills

Effect parameters use the generic skill-effect model in `interfaces.md`.

| id | name | mp | learn level | gold | key item | effect |
|---|---|---|---|---|---|---|
| `angel_wings` | Angel WINGS | 10 | 5 | 1000 | `topaz` | `healToFull` |
| `meteor_fist` | Meteor FIST | 25 | 5 | 5000 | `gold_leaf` | damage, atkMultiplier 2 |
| `golden_bell` | Golden BELL | 25 | 10 | 10000 | `frozen_tears` | damage ×1, heal 50% of damage dealt, self status `reflect` 2 |
| `frozen_throne` | FROZEN Throne | 40 | 15 | 20000 | `broken_staff` | damage ×1, target `stun` 3 and `defense_down` 3; rejected if target already stunned |
| `supernova` | SUPERNOVA | 50 | 20 | 50000 | `pure_ice` | halve target current HP first, then damage ×5 ignoring DEF |

Descriptions: "Greater Healing", "Greater Damage", "Greater Armor", "Deep Freeze and Lower Defense", "Ultimate Destruction and Armor Destroyer".

**[decided] Skill effects.** The original code for skills 2–4 was placeholder copy-paste (damage = 0.8 × current HP), and skill 5 was never written. The effects above implement the author's design comment in `main.cpp`: "heal own self, double dmg, life steal + reflect, stun (triple turn) + lower defense, halve boss HP + 5× damage + zero armor".

**[decided] Learning cost.** The original checked gold but never deducted it, and never consumed the key item. Learning now deducts the gold and consumes 1 key item.

## Status effects

| id | effect while active |
|---|---|
| `stun` | the owner skips its turn |
| `defense_down` | the owner's DEF is halved (integer floor) for damage calculation |
| `reflect` | incoming damage `d` is split: the owner takes `d − d/2` and the attacker takes `d/2` (integer division) |

Timing rules are in `interfaces.md` → Battle turn rules.

## Enemies

### Medusa (mission boss)

| field | value |
|---|---|
| id | `medusa` |
| name | Medusa |
| maxHp | 3000 |
| atk | 250 |
| def | 20 |
| life | 10 |
| behavior | each enemy turn: `below(10) < 5` → **Strike**: attack, then regenerate `life` HP (capped at max). Otherwise → **Gaze**: attack, then `below(10) < 6` grants an immediate extra enemy turn |
| rewards | 10000 exp, 25000 gold, 1 × `thors_hammer` (guaranteed, no drop rolls) |
| sprite | `assets/sprites/medusa.sprite` |

### Training monsters (generated, not in enemies.json)

For cave level `L` (1–30), each round's monster is computed with the training RNG in this exact draw order:

1. `atk = (15 + below(5)) × 2 × L`
2. `def = (5 + below(5)) × L`
3. `goldReward = (10 + below(10)) × 10 × L`

Then `maxHp = hp = 100 × L`, `expReward = 100 × L`, `life = 0`.

Name tier `t = L / 10 + 1` (integer division, so 1–4). Names by tier: 1 Fox, 2 Bat, 3 Horse, 4 Bull. **[decided]** The original indexed a 10-name array by this tier, so only these four names were reachable.

## Missions

| id | name | location | min level | repeatable | enemy | story lines |
|---|---|---|---|---|---|---|
| `the_gorgon` | Mission ONE: "The Gorgon" | MEDUSA LAIR | 20 | no | `medusa` | "Bring me the head of Medusa..." / "...and such artifact never been seen will be yours!" |

Missions 2 ("Lake of Lerna") and 3 ("The Twin") are listed in the menu as "Coming Soon" and cannot be selected.

## Forge recipes

| id | result | ingredients |
|---|---|---|
| `forge_fire_claymore` | `fire_claymore` | 1 `iron_sword`, 1 `phoenix_feather`, 1 `fire_dust` |
| `forge_peace_maker` | `peace_maker` | 1 `steel_shield`, 1 `broken_compass`, 1 `black_pearl`, 1 `gold_dust` |

Ingredients must be in the bag. Equipped items do not count.

## Economy formulas

| rule | formula |
|---|---|
| exp to next level after reaching level `L` | `L × 100 × multiplier` |
| upgrade at current level `L` (max 10) | consumes `L` × material, `1000 × L` exp, `10000 × L` gold |
| forge success | `below(100) < forgeLevel × 10`. On success `forgeLevel = min(10, forgeLevel + 1)` |
| charisma (level ≥ 10) | input spend `E` exp. `points = E / 50`. Gold cost `points × 100`. Gain `points × 10 × multiplier / level` (integer, computed left to right). Refuse with nothing deducted if `points < 1` or gain `< 1` |
| revive (entering guildmaster at 0 HP) | costs `min(100, gold)`, sets `hp = min(100, maxHp)` |
| rename | 50000 gold |
| bank account | 10000 gold, one-time |
| training round count | 1–100 |
| auction | see `interfaces.md` → Town rules |

**[decided] Charisma:** the original consumed the EXP even when `points < 1`. That is fixed.

**[decided] Revive:** the original left a broke dead hero soft-locked forever, so revive is now free when the hero has under 100 gold.

**[decided] Upgrade:** the original checked for `L` materials but consumed only 1. It now consumes `L`.

**[decided] Forge:** the original used two different success formulas. Both recipes now use the single formula above. Ingredients are consumed on failure too, as in the original.

## Story text

Opening (typewriter, 7 lines): "One of the first seven to become a hero, and one of the few who had been chosen by the sacred light" / "They called him the bravest of all man, ever since he partook in the Great Quest" / "He hunted monsters even if it did not bring him any benefits" / "He annihilated the dangerous dungeons that everyone else avoided" / "and he also judged heroes and guilds who defied the law" / "only he was different..." / "Therefore, he will be the one to save the world....."

Path intro (4 lines): "Among all the hero..." / "a Fighter is blessed with the Strongest Fist" / "a Hunter with the Body Strong as rock" / "while a Rogue will have a Balance Mind..."

Game title: "Ancient Warrior RPG".
