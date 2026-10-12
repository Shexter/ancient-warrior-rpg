# combat Specification

## Purpose
Resolves turn-based battles between the hero and enemies deterministically, and reports every outcome as a structured event so the UI, the simulator, and later the narrator can present it.

## Requirements

### Requirement: Integer damage formula
All battle arithmetic SHALL use integers, with division truncating toward zero. Basic attack damage SHALL be `max(0, attacker ATK − defender DEF)`. The hero's ATK, DEF, and LIFE SHALL be base stats plus each equipped item's bonus multiplied by that item's upgrade level.

#### Scenario: Armor absorbs weak hit
- **WHEN** an attacker with 100 ATK hits a defender with 120 DEF
- **THEN** the defender takes 0 damage and a damage event with amount 0 is reported

#### Scenario: Equipment upgrade scales attributes
- **WHEN** a Fighter (50 ATK) wields an Iron Sword (50 ATK) at upgrade level 2
- **THEN** the hero's effective ATK is 150

### Requirement: Deterministic battles
Given the same seed, the same starting state, and the same sequence of player actions, a battle SHALL produce the same event sequence and outcome on macOS, Linux, and Windows.

#### Scenario: Golden sequence
- **WHEN** a starting Fighter (100 ATK, 25 DEF, 350 HP after equipment) always attacks Medusa with battle seed stream (1, 0)
- **THEN** the battle ends in defeat on turn 2 after the hero takes 225 damage twice, matching the recorded golden event sequence

### Requirement: Battle event stream
Every state change in a battle SHALL be reported as an ordered event: battle started, turn started, turn skipped, attacked, skill cast, HP halved, damaged, healed, status applied, status expired, extra turn, fled, defeated, battle ended. Combat logic SHALL NOT print, render, sleep, or read input.

#### Scenario: Attack emits events
- **WHEN** the hero attacks and deals 80 damage to Medusa at 3000 HP
- **THEN** the event stream contains a hero turn-started event, an attacked event, and a damaged event with amount 80 and remaining HP 2920

### Requirement: Player actions in mission battles
In mission battles the player SHALL choose Attack, Skill, or Flee each turn. Attack SHALL heal the hero by its LIFE after hitting if the enemy survives. A skill SHALL be usable only if learned, affordable in MP, and its preconditions hold. A rejected action SHALL produce no events and change no state, and it SHALL remain the hero's turn. Flee SHALL end the battle immediately with no rewards and no penalty.

#### Scenario: Not enough mana
- **WHEN** the hero with 5 MP selects Angel Wings (10 MP)
- **THEN** the action is rejected, MP is unchanged, and it is still the hero's turn

#### Scenario: Frozen Throne while enemy frozen
- **WHEN** the hero casts Frozen Throne while the enemy is still stunned
- **THEN** the action is rejected as not ready and no MP is spent

### Requirement: Skills
The five skills SHALL have these effects: Angel Wings heals the hero to full. Meteor Fist deals `max(0, 2×ATK − DEF)`. Golden Bell deals `max(0, ATK − DEF)`, heals the hero by half the damage dealt, and grants Reflect for 2 turns. Frozen Throne deals `max(0, ATK − DEF)` and applies Stun 3 and Defense Down 3 to the enemy. Supernova halves the enemy's current HP, then deals `5×ATK` ignoring DEF.

#### Scenario: Supernova
- **WHEN** a hero with 300 ATK casts Supernova on Medusa at 3000 HP
- **THEN** Medusa's HP is halved to 1500, then she takes 1500 damage and is defeated

### Requirement: Status effects with durations
Stun SHALL make its owner skip its turns. Defense Down SHALL halve its owner's DEF in damage calculations. Reflect SHALL split incoming damage so the owner takes the larger half and the attacker takes the smaller half (integer division by 2). A status SHALL count down at the end of each of its owner's turns, except the turn on which the owner applied it to itself. It SHALL expire at zero with an event. Re-applying an active status SHALL reset its duration rather than stack.

#### Scenario: Frozen Throne timing
- **WHEN** the hero casts Frozen Throne on turn 1 and then attacks
- **THEN** the enemy skips exactly its next 3 turns, the hero's attacks on turns 2–4 use the enemy's halved DEF, and both statuses expire at the end of the enemy's third skipped turn

#### Scenario: Reflect
- **WHEN** the hero has Reflect and Medusa's attack would deal 50 damage
- **THEN** the hero takes 25 damage and Medusa takes 25 reflected damage

### Requirement: Enemy behavior
Medusa SHALL, on each of her turns, attack with a 50% chance of then regenerating her LIFE in HP, and otherwise attack with a 60% chance of immediately taking another turn.

#### Scenario: Extra turn
- **WHEN** Medusa's behavior roll selects the gaze and the extra-turn roll succeeds
- **THEN** an extra-turn event is emitted and Medusa acts again before the hero

### Requirement: Turn cap
Any battle, whether mission or training, that reaches 1,000 hero turns (training: 1,000 exchanges) without a winner SHALL end as a draw with no rewards and no penalty.

#### Scenario: Unwinnable stalemate
- **WHEN** neither combatant can deal damage
- **THEN** the battle ends as a draw after 1,000 turns

### Requirement: Mission outcome
Victory in a mission SHALL grant the enemy's EXP, gold, and reward items and mark the mission completed. Defeat SHALL set the hero's HP to 0 and destroy the equipped weapon and armor. In every outcome the hero SHALL keep the HP and MP they ended the battle with.

#### Scenario: Defeat destroys gear
- **WHEN** the hero is defeated by Medusa
- **THEN** the hero has no weapon and no armor equipped and has 0 HP
