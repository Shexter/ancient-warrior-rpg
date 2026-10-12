# Spec Delta

## Purpose

Tells the player what to do next and how likely a fight is to go well, using the same deterministic estimate the balance tools use, so progress never depends on reading the docs.

## ADDED Requirements

### Requirement: Win-chance estimates
The game SHALL estimate the hero's chance of winning a training round at each cave level and each mission battle by simulating the fight with the `smart` policy on copies of the current state, without touching the game's random stream or state. The estimate SHALL be deterministic for a given state and shown as a whole percentage.

#### Scenario: Estimate leaves the game untouched
- **WHEN** the training cave list shows estimates for all 30 caves
- **THEN** the game state, including its random stream, is identical to before

#### Scenario: Stable estimate
- **WHEN** the same state is estimated twice
- **THEN** both estimates are equal

### Requirement: Next goal
The Hub SHALL show one "Next:" line naming the most useful next step, chosen in this order: revive at the Prior; spend unspent blessing points; for the first uncompleted Act I mission, train to its level if below it, or name it with the estimated win chance and, below 50%, suggest getting stronger; after Act I, the bonus mission; then "Act I is complete."

#### Scenario: Fallen hero
- **WHEN** the hero has 0 HP
- **THEN** the Hub says "Next: visit the Prior to be revived"

#### Scenario: Not ready for a boss
- **WHEN** a level 15 hero with no unspent points has a 30% estimated chance against Avaritia
- **THEN** the Hub says "Next: Avaritia (Lv 15) — your odds ≈30%; upgrade gear, learn skills, or train"
