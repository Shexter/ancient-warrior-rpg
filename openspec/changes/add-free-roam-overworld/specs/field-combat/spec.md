# Spec Delta

## Purpose

Real-time fights against monsters roaming the Overworld. Positioning, dodging telegraphed hits and timing cooldowns decide them, and they use the game's existing damage rule, monster tables and rewards.

## ADDED Requirements

### Requirement: Zones and monsters
Each zone SHALL have a level from 1 to 30 and a monster count. A zone's monsters SHALL be rolled with the training-monster roll at the zone's level, giving their name, HP, ATK, DEF, gold and EXP. A defeated monster SHALL respawn after a fixed delay (set in the field data) on a walkable tile in its zone that is outside the hero's view. Monsters SHALL stay inside their zone.

#### Scenario: Zone level sets stats
- **WHEN** a level 5 zone spawns a monster
- **THEN** its stats and rewards are those the training-monster roll gives at level 5

#### Scenario: Respawn out of sight
- **WHEN** a monster is defeated and the respawn delay passes
- **THEN** a newly rolled monster appears in that zone on a tile the hero cannot see

### Requirement: Monster behavior and telegraphs
A monster SHALL wander its zone until the hero comes within its notice radius. It SHALL then step toward the hero along a walkable path until the hero leaves the zone or gets beyond twice that radius. A monster next to the hero SHALL wind up an attack on the hero's tile, shown on screen for the whole wind-up time. When the wind-up ends it SHALL hit only if the hero is still on that tile, then wait out its attack cooldown.

#### Scenario: Dodging
- **WHEN** a monster begins its wind-up and the hero steps to another tile before it ends
- **THEN** the hero takes no damage

#### Scenario: Standing still
- **WHEN** a monster's wind-up ends with the hero on the targeted tile
- **THEN** the hero takes that monster's hit

### Requirement: Damage
Every field hit SHALL deal its power minus the target's DEF, with a minimum of 0. A monster's power SHALL be its ATK. The hero's DEF SHALL include equipment bonuses, as in battle. Each hit SHALL show its damage over the target for a moment. The hero SHALL regain HP equal to their Life stat at a fixed interval set in the field data, up to their maximum HP, and Fervor at a fixed rate.

#### Scenario: Armor absorbs a weak hit
- **WHEN** a monster with ATK 20 hits a hero with total DEF 25
- **THEN** the hero takes 0 damage and "0" is shown over the hero

### Requirement: Hero abilities
The hero SHALL have these abilities, each with a key, a Fervor cost, a cooldown and a power multiple of the hero's ATK (including equipment), all set in the field data:
- **Swing:** Space, or clicking a map cell. It hits a monster on the tile the hero faces. A click first turns the hero toward the clicked cell along the axis with the larger distance.
- **Firebolt:** key 1. A bolt flies from the hero in the facing direction and hits the first monster in its path. It stops at a tile that can't be walked on or after its maximum range.
- **Nova:** key 2. It hits every monster within its radius of the hero.

An ability used without enough Fervor or before its cooldown has passed SHALL do nothing and briefly flash that hotbar slot.

#### Scenario: Swing at an adjacent monster
- **WHEN** the hero faces an adjacent monster and presses Space
- **THEN** that monster takes the hero's ATK minus its DEF

#### Scenario: Not enough Fervor
- **WHEN** the hero has less Fervor than Nova costs and presses 2
- **THEN** no monster is hit, Fervor is unchanged, and the Nova slot flashes

#### Scenario: Bolt blocked by a tree
- **WHEN** a tree stands between the hero and a monster in the facing direction and the player presses 1
- **THEN** the bolt stops at the tree and the monster takes no damage

### Requirement: Potions in the field
Q SHALL use the first consumable in the bag that restores HP, following the same rules as using it at the Prior, and SHALL remove it from the bag. With none, Q SHALL show "No potions" and change nothing.

#### Scenario: Drinking a potion
- **WHEN** the hero has one HP potion and presses Q
- **THEN** HP is restored by that potion and the bag no longer holds it

### Requirement: Rewards
A monster brought to 0 HP SHALL be removed and give its gold and EXP. EXP SHALL go through the game's normal EXP grant, so level-ups and blessing points happen as anywhere else. A drop SHALL be rolled at the zone's level. A drop that doesn't fit a full bag SHALL be lost, with a message saying so.

#### Scenario: Level up in the field
- **WHEN** a monster's EXP takes the hero past the next level
- **THEN** the hero levels up, gains a blessing point, and a level-up message is shown

### Requirement: Field defeat
When the hero's HP reaches 0 in the field, the hero SHALL lose EXP equal to the EXP reward of the monster that landed the hit (not below 0). Equipment, gold and items SHALL be kept. The Overworld SHALL close and return to the Hub with the message "You fell in the field. Visit the Prior to be revived."

#### Scenario: Falling
- **WHEN** a monster's hit takes the hero from 10 HP to 0
- **THEN** the Hub is shown with that message, the hero has 0 HP, and Explore, Training and Mission are disabled
