# Act I names and text

The authoritative player-facing text for `reskin-act-one`. Numbers (stats, prices, rates, rewards) stay as in `openspec/changes/archive/2026-10-11-build-core-engine/reference/content-tables.md`, except the new Avaritia rows at the end. IDs never change.

Display names are English, and the Latin goes first in the description, so both are seen (world-bible §7: "Latin terms for named things"). Latin is a first pass and is reviewed by the user (world-bible §9.3).

## Title

`story.json` `title`: **Ordo Lucis**

## Paths (`paths.json`)

| id | name | flavor |
|---|---|---|
| `fighter` | Knight | a knight of the Order, blessed with the Strongest Fist |
| `hunter` | Hunter | a ranger who tracks the demon-touched, a body strong as rock |
| `rogue` | Pilgrim | a wandering scholar-confessor, a balanced mind |

## Items (`items.json`)

| id | name | description |
|---|---|---|
| `iron_sword` | Sword | Gladius. A plain blade of the Order. |
| `fire_claymore` | Flaming Sword | Gladius Flammeus. Like the blade that guards Eden's gate. |
| `thors_hammer` | Mace of St. Michael | Clava Sancti Michaelis. The archangel's weapon against the dragon. |
| `steel_shield` | Iron Shield | Scutum Ferreum. Dented, but it holds. |
| `diamond_plate` | Adamant Cuirass | Lorica Adamantina. Plate that does not yield. |
| `peace_maker` | Armor of Light | Arma Lucis. "Let us put on the armor of light." |
| `topaz` | Rosary | Rosarium. A prayer made physical. |
| `gold_leaf` | Martyr's Relic | Reliquia Martyris. Kept by those who remember. |
| `frozen_tears` | Tears of Penance | Lacrimae Paenitentiae. Sorrow turned to strength. |
| `broken_staff` | Broken Crozier | Baculus Fractus. A shepherd's staff, snapped in a fight. |
| `pure_ice` | Fragment of the Cross | Fragmentum Crucis. Handle with reverence. |
| `fire_dust` | Altar Ember | Favilla. Still warm from the altar. Hardens a blade. |
| `turtle_shell` | Consecrated Plate | Lamina Sacra. Blessed iron for armor. |
| `gold_dust` | Gold Dust | Pulvis Aureus. Gilding from old icons. |
| `black_pearl` | Dark Pearl | Margarita Obscura. Beautiful, and a temptation. |
| `broken_compass` | Broken Compass | Acus Fracta. A pilgrim's lost guide. |
| `phoenix_feather` | Phoenix Feather | Penna Phoenicis. A sign of resurrection. |
| `health_potion` | Holy Water | Aqua Benedicta. Restores HP. |
| `mana_potion` | Holy Oil | Oleum Sanctum. Restores Fervor. |
| `eternal_youth` | Viaticum | Viaticum. Food for the journey. Restores HP and Fervor fully. |

## Skills (`skills.json`)

| id | name | description |
|---|---|---|
| `angel_wings` | Guardian Angel | Angelus Custos. Heals to full. |
| `meteor_fist` | Righteous Strike | Ictus Iustus. A double-strength blow. |
| `golden_bell` | Shield of Faith | Scutum Fidei. Turns damage back on the attacker. |
| `frozen_throne` | Exorcism | Exorcizo Te. Binds the enemy and weakens its guard. |
| `supernova` | Let There Be Light | Fiat Lux. Halves the enemy, then strikes through armor. |

## Story (`story.json`)

`opening` (7 lines) and `pathIntro` (4 lines): exactly world-bible §8.

## Missions and enemies

| mission id | name | location | enemy | minLevel | repeatable | story |
|---|---|---|---|---|---|---|
| `avaritia` | Act I: "Avaritia" | THE COUNTING HOUSE | `avaritia` | 15 | no | "Greed has made a house of the old exchange." / "Whatever it takes, it keeps. Take it back." |
| `the_gorgon` | Bonus: "The Gorgon" | MEDUSA LAIR | `medusa` | 20 | no | unchanged |

Missions are listed in that order. Medusa's enemy entry is unchanged.

New enemy `avaritia`:

| field | value |
|---|---|
| name | Avaritia |
| sprite | `avaritia.sprite` (32×28) |
| behavior | `medusa_pattern` |
| maxHp / atk / def / life | starting point 2200 / 150 / 15 / 5, final values set by the balance band (balance-simulator spec) |
| rewardExp / rewardGold | 5000 / 15000 |
| rewardItems | `gold_dust`, `black_pearl` (two of the Armor of Light's forge ingredients: what greed hoarded becomes protection) |

## Player-facing vocabulary (code text)

| where | before | after |
|---|---|---|
| status panel, battle row 17 | `MP` | `FV` |
| Create Hero preview | `MP   50` | `Fervor 50` |
| battle skill overlay | `Name (MP)` | `Name (Fervor cost)` column, values unchanged |
| rejected skill | "Not enough MP." | "Not enough Fervor." |
| Charisma stat menu | Max MP | Max Fervor |
| Restoration result | `HP a/b · MP c/d` | `HP a/b · Fervor c/d` |
| Town title / menu | TOWN; Merchant, Blacksmith, Guildmaster, Bank | CIVITAS LUCIS; Almonry, Forge, Prior, Thesaurus |
| NPC screen titles | MERCHANT, BLACKSMITH, GUILDMASTER, BANK | ALMONRY, FORGE OF THE ORDER, THE PRIOR, THESAURUS |
| Training titles | TRAINING CAVE, TRAINING RESULTS | THE CATACOMBS, CATACOMB RESULTS |
| Hub disabled reason | Fallen: visit the Guildmaster | Fallen: visit the Prior |
| Almonry, fallen hero | I have no business with you! | The Almonry serves the living. See the Prior first. |
| Forge, fallen hero | Empty soul, you don't belong here | No blade for an empty soul. See the Prior first. |
| Prior, revive | Great warrior, be careful next time... (revived for N gold) | Rise. Mercy asks only that you be more careful. (N gold to the poor box) |
| Thesaurus, no account | Open an account for 10000 gold? | Open a place in the Thesaurus for 10000 gold? |

Unchanged on purpose: "HP", "ATK", "DEF", "LIFE", "EXP", "Gold", the Hub's Training / Mission / Town / Save / Quit labels (function words the player navigates by), the training monster names (Fox, Bat, Horse, Bull), and all of Medusa.
