# Spec Delta

## MODIFIED Requirements

### Requirement: Act I mission list
The shipped missions SHALL be, in order, the Act I missions against Avaritia (level 15), Gula (level 18) and Ira (level 22), then the bonus mission against Medusa (level 20). Each Act I boss SHALL have its own script that telegraphs its strongest action with the Charging status a turn ahead; Medusa's enemy definition and script SHALL be unchanged.

#### Scenario: Mission order
- **WHEN** a level 22 hero opens Mission Select
- **THEN** the rows are `Act I: "Avaritia"`, `Act I: "Gula"`, `Act I: "Ira"`, `Bonus: "The Gorgon"`, then the two "Coming Soon" rows

#### Scenario: Earlier unlock
- **WHEN** a level 15 hero with a weapon and armor views the Hub
- **THEN** Mission is enabled, and below level 15 it is disabled with "Requires Lv 15"

#### Scenario: Medusa unchanged
- **WHEN** the Medusa golden-vector tests run against the shipped data
- **THEN** they pass without any change to the vectors

## ADDED Requirements

### Requirement: Act I skills
The shipped content SHALL include three scripted skills beyond the original five: Smite (deals ATK, tripled if the enemy is charging), Sanctuary (Guard for 2 turns and heals 20% of max HP), and Judgment (deals 3×ATK ignoring DEF if the enemy is below a quarter of its max HP, otherwise ATK), with costs and key items in `reference/fun-pass-tables.md`.

#### Scenario: Smite a charging boss
- **WHEN** a hero with 300 ATK casts Smite on a charging enemy with 0 DEF
- **THEN** the enemy takes 900 damage
