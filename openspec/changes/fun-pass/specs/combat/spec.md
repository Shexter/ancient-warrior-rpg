# Spec Delta

## MODIFIED Requirements

### Requirement: Player actions in mission battles
In mission battles the player SHALL choose Attack, Skill, Item, Defend, or Flee each turn. Attack SHALL heal the hero by its LIFE after hitting if the enemy survives. A skill SHALL be usable only if learned, affordable in MP, and its preconditions hold. Item SHALL use one consumable from the hero's battle stock (the consumables in the bag when the battle started) with its normal effect; items used SHALL be removed from the bag when the battle ends, whatever the outcome. Defend SHALL give the hero Guard until its next turn and restore 10 MP (capped at max MP). A rejected action SHALL produce no events and change no state, and it SHALL remain the hero's turn. Flee SHALL end the battle immediately with no rewards and no penalty.

#### Scenario: Not enough mana
- **WHEN** the hero with 5 MP selects Angel Wings (10 MP)
- **THEN** the action is rejected, MP is unchanged, and it is still the hero's turn

#### Scenario: Frozen Throne while enemy frozen
- **WHEN** the hero casts Frozen Throne while the enemy is still stunned
- **THEN** the action is rejected as not ready and no MP is spent

#### Scenario: Holy Water mid-battle
- **WHEN** the hero at 40 of 350 HP with two Holy Water in the bag uses one in battle and later wins
- **THEN** the hero heals 100 HP that turn, the enemy then acts, and after the battle the bag holds one Holy Water

#### Scenario: Item not in stock
- **WHEN** the hero selects an item it has none of
- **THEN** the action is rejected with no events and it is still the hero's turn

#### Scenario: Defend against a charged strike
- **WHEN** the hero defends on the turn a charging boss strikes
- **THEN** the strike's damage to the hero is halved and the hero has 10 more MP than before defending

### Requirement: Status effects with durations
Stun SHALL make its owner skip its turns. Defense Down SHALL halve its owner's DEF in damage calculations. Reflect SHALL split incoming damage so the owner takes the larger half and the attacker takes the smaller half (integer division by 2). Guard SHALL halve damage its owner takes (integer division by 2, applied before Reflect). Charging SHALL have no effect of its own: it is a visible warning that an enemy's script uses to wind up a strong action. A status SHALL count down at the end of each of its owner's turns, except the turn on which the owner applied it to itself. It SHALL expire at zero with an event. Re-applying an active status SHALL reset its duration rather than stack.

#### Scenario: Frozen Throne timing
- **WHEN** the hero casts Frozen Throne on turn 1 and then attacks
- **THEN** the enemy skips exactly its next 3 turns, the hero's attacks on turns 2–4 use the enemy's halved DEF, and both statuses expire at the end of the enemy's third skipped turn

#### Scenario: Reflect
- **WHEN** the hero has Reflect and Medusa's attack would deal 50 damage
- **THEN** the hero takes 25 damage and Medusa takes 25 reflected damage

#### Scenario: Guard lasts one enemy phase
- **WHEN** the hero defends on turn 1
- **THEN** every hit on the hero during the enemy's turns that follow is halved, and Guard expires at the end of the hero's turn 2
