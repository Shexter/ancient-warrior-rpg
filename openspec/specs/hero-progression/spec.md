# hero-progression Specification

## Purpose
Covers how a hero is created and grows: paths, experience and leveling, equipment and upgrades, inventory, training, missions, and the consequences of death.

## Requirements

### Requirement: Hero creation
A new game SHALL ask for a hero name of 1–8 printable ASCII characters (leading and trailing spaces trimmed) and a path (Fighter, Hunter, Rogue). The hero SHALL start at level 1 with the path's base stats, full HP and MP, 20,000 gold, 0 EXP, the Iron Sword and Steel Shield equipped at upgrade level 1, an empty 10-slot bag, no bank account, no skills, and forge level 9.

#### Scenario: Name too long
- **WHEN** the player enters a 12-character name
- **THEN** the game asks again and explains the 8-character limit

#### Scenario: Fighter starting stats
- **WHEN** a Fighter is created
- **THEN** it has 350 max HP, 50 max MP, 50 ATK, 5 DEF, and 0 LIFE before equipment

### Requirement: Two kinds of experience
Gaining EXP SHALL both advance level progress and add to a spendable EXP balance. Spending EXP (upgrades, Charisma) or losing it (training death) SHALL reduce only the spendable balance and SHALL never lower the hero's level.

#### Scenario: Spending does not de-level
- **WHEN** a level 14 hero spends 1,000 EXP on an upgrade
- **THEN** the hero is still level 14 with the same progress toward level 15

### Requirement: Leveling
EXP needed to go from level L to L+1 SHALL be `L × 100 × path multiplier` (Fighter 1, Hunter 2, Rogue 3). A single gain SHALL be able to raise several levels, and any remainder carries toward the next. Leveling SHALL NOT change stats.

#### Scenario: Multi-level gain
- **WHEN** a new Fighter gains 10,000 EXP
- **THEN** the hero is level 14 and needs 500 more EXP for level 15

### Requirement: Inventory
The bag SHALL have 10 ordered slots in which identical items stack without limit. Adding an item SHALL fail when there is no existing stack and no empty slot. A failed add SHALL change nothing, and the player SHALL be told about any item lost this way.

#### Scenario: Full bag drop
- **WHEN** a training drop occurs with all 10 slots holding different items
- **THEN** the round summary reports the item was lost because the bag is full

### Requirement: Adventure gate and death
The hero SHALL need both a weapon and armor equipped and more than 0 HP to enter training or missions. A hero at 0 HP SHALL be refused by the merchant and blacksmith until revived at the guildmaster.

#### Scenario: Dead hero blocked
- **WHEN** a hero with 0 HP selects Training
- **THEN** entry is refused with a message to visit the Guildmaster

### Requirement: Training cave
Training SHALL take a cave level from 1 to 30 and a round count from 1 to 100, and fight one generated monster per round automatically. HP SHALL carry over between rounds. Each win SHALL grant `(10–19) × 10 × L` gold, `100 × L` EXP, and one drop roll. A defeat SHALL set HP to 0, remove that round's EXP reward from the spendable balance (not below 0), and end the session. A draw SHALL grant nothing and continue with the next round.

#### Scenario: Invalid cave level
- **WHEN** the player enters cave level 31
- **THEN** the game rejects it and shows the valid range

#### Scenario: Death ends training
- **WHEN** the hero falls in round 3 of 10
- **THEN** rounds 4–10 are not fought and the hero has 0 HP

### Requirement: Drop roll
A drop roll SHALL pick one droppable item uniformly at random, then award it with probability `min(100, drop rate + cave level)` percent. It SHALL always terminate, even if no items are droppable.

#### Scenario: Empty drop table
- **WHEN** content defines no droppable items
- **THEN** drop rolls award nothing and training completes normally

### Requirement: Missions
Missions SHALL unlock at their minimum level (20 for "The Gorgon"), require a living, equipped hero, and be completable once unless marked repeatable.

#### Scenario: Locked mission
- **WHEN** a level 19 hero opens Missions
- **THEN** "The Gorgon" is shown as locked with "Requires Lv 20"

#### Scenario: Cleared mission
- **WHEN** a hero who defeated Medusa opens Missions
- **THEN** "The Gorgon" is shown as cleared and cannot be started
