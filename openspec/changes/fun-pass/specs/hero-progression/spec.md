# Spec Delta

## MODIFIED Requirements

### Requirement: Leveling
EXP needed to go from level L to L+1 SHALL be `L × 100 × path multiplier` (Fighter 1, Hunter 2, Rogue 3). A single gain SHALL be able to raise several levels, and any remainder carries toward the next. Leveling SHALL NOT change stats directly; each level gained SHALL grant one blessing point, which the player spends at the Prior.

#### Scenario: Multi-level gain
- **WHEN** a new Fighter gains 10,000 EXP
- **THEN** the hero is level 14, needs 500 more EXP for level 15, and has 13 unspent blessing points, with stats unchanged

### Requirement: Training cave
Training SHALL take a cave level from 1 to 30 and then fight one generated monster per round, one round at a time; after each round the player SHALL choose to continue or leave. HP SHALL carry over between rounds. Each win SHALL grant `(10–19) × 10 × L` gold, `100 × L` EXP, and one drop roll, and SHALL add `round gold × min(streak, 10) / 10` to a pot, where streak is the number of wins earlier in the session. Leaving SHALL add the pot to the hero's gold. A defeat SHALL set HP to 0, remove that round's EXP reward from the spendable balance (not below 0), forfeit the pot, and end the session. A draw SHALL grant nothing, reset the streak, and let the player continue or leave. Each session SHALL fork the game's random stream once.

#### Scenario: Invalid cave level
- **WHEN** the player enters cave level 31
- **THEN** the game rejects it and shows the valid range

#### Scenario: Banking the pot
- **WHEN** the hero wins three rounds at cave 1 earning 150, 120 and 180 gold, then leaves
- **THEN** the hero has gained 450 gold from the rounds plus a 12 + 36 = 48 gold pot

#### Scenario: Death ends training
- **WHEN** the hero wins two rounds and falls in the third
- **THEN** no further rounds are fought, the two rounds' gold and EXP are kept, the pot is lost, and the hero has 0 HP
