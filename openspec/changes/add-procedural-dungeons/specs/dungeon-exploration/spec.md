# Spec Delta

## Purpose

Lets the hero explore a generated dungeon turn by turn: seeing only what is in line of sight, being pursued by enemies, fighting through the existing combat engine, collecting loot, and descending or leaving.

## ADDED Requirements

### Requirement: Entry gate
Starting an expedition SHALL require the same conditions as training (weapon and armor equipped, HP above 0).

#### Scenario: Dead hero refused
- **WHEN** a hero at 0 HP selects Expedition
- **THEN** entry is refused with a message to visit the Guildmaster

### Requirement: Turn-based movement
The hero SHALL move one tile per keypress in four directions (arrow keys or WASD) and SHALL NOT enter walls. Each hero move or wait SHALL give every awake enemy one move.

#### Scenario: Wall blocks movement
- **WHEN** the hero moves into a wall
- **THEN** the hero's position is unchanged and no turn passes

### Requirement: Field of view and memory
Only tiles in the hero's line of sight within radius 8 SHALL be shown at full brightness. Previously seen tiles SHALL be shown dimmed. Enemies SHALL be shown only while visible.

#### Scenario: Wall hides room
- **WHEN** a wall lies between the hero and a room
- **THEN** that room's tiles and enemies are not shown unless previously seen

### Requirement: Enemy pursuit
An enemy that sees the hero SHALL wake and move along a shortest walkable path toward the hero each turn. An enemy with no path SHALL stay in place.

#### Scenario: Shortest path pursuit
- **WHEN** an awake enemy is 5 walkable steps from the hero around a corner
- **THEN** after the hero waits 4 turns the enemy is adjacent to the hero

### Requirement: Encounters use combat engine
Moving into an enemy, or an enemy moving into the hero, SHALL start a battle using the existing combat rules. Victory removes the enemy from the map and grants its rewards; defeat ends the expedition with the training death rules.

#### Scenario: Bump to fight
- **WHEN** the hero moves into an adjacent enemy
- **THEN** the battle screen opens with that enemy

### Requirement: Loot, descent, and leaving
Opening a chest SHALL grant items from its floor's chest table using the inventory rules. Using the down-staircase SHALL generate and enter the next floor. The hero MAY leave from the entry staircase of floor 1 at any time and keep all loot.

#### Scenario: Leave with loot
- **WHEN** the hero opens a chest on floor 1 and returns to the entry staircase to leave
- **THEN** the hero is back in town with the chest's items in the bag

### Requirement: Expedition persistence
Saving during an expedition SHALL persist the dungeon seed, current floor, hero position, explored tiles, and remaining enemies and chests, so loading resumes the same situation. Saves from before this change SHALL still load.

#### Scenario: Resume expedition
- **WHEN** the player saves on floor 2 and later loads
- **THEN** the hero is on floor 2 at the same position with the same explored area and enemies
