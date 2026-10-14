# Spec Delta

## MODIFIED Requirements

### Requirement: Act I mission list
The shipped missions SHALL be, in order, the Act I missions against Avaritia (level 15), Gula (level 18) and Ira (level 22), then the bonus mission against Medusa (level 20). Each Act I boss SHALL have its own script that telegraphs its strongest action with the Charging status a turn ahead, following the behaviour and stats in `reference/fun-pass-tables.md`. Whether a boss telegraphs on a given turn SHALL be a roll (Avaritia 1 in 4, Gula 1 in 3, Ira 1 in 2 and only below half HP), not a fixed cadence, and every boss hit SHALL vary by ±15% (`power × (85 + roll 0–30) / 100`, rounded down), so outcomes depend on play rather than being all-or-nothing. Medusa's enemy definition and script SHALL be unchanged.

#### Scenario: Mission order
- **WHEN** a level 22 hero opens Mission Select
- **THEN** the rows are `Act I: "Avaritia"`, `Act I: "Gula"`, `Act I: "Ira"`, `Bonus: "The Gorgon"`, then the two "Coming Soon" rows

#### Scenario: Earlier unlock
- **WHEN** a level 15 hero with a weapon and armor views the Hub
- **THEN** Mission is enabled, and below level 15 it is disabled with "Requires Lv 15"

#### Scenario: Telegraph then strike
- **WHEN** Avaritia (ATK 100 against a hero with 0 DEF) telegraphs
- **THEN** that turn it says "Avaritia counts its coins...", gains Charging for one turn and deals no damage, and on its next turn it attacks for 212–287 (2.5 × ATK ± 15%) and Charging expires

#### Scenario: Gula's drain
- **WHEN** Gula hits the hero with a normal attack, or with its devour after charging
- **THEN** it heals half the damage dealt (rounded down), or all of it for the devour

#### Scenario: Ira's rage
- **WHEN** Ira, with 100 ATK and 5000 max HP, attacks at 1400 HP
- **THEN** its power is 172 (ATK × (100 + 72) / 100) before the ±15% variation, and it may charge only because it is below half HP

#### Scenario: Medusa unchanged
- **WHEN** the Medusa golden-vector tests run against the shipped data
- **THEN** they pass without any change to the vectors

## ADDED Requirements

### Requirement: Act I skills
The shipped content SHALL include three scripted skills beyond the original five: Smite (deals ATK, tripled if the enemy is charging), Sanctuary (Guard for 2 turns and heals 20% of max HP), and Judgment (deals 3×ATK ignoring DEF if the enemy is below a quarter of its max HP, otherwise ATK), with costs and key items in `reference/fun-pass-tables.md`.

#### Scenario: Smite a charging boss
- **WHEN** a hero with 300 ATK casts Smite on a charging enemy with 0 DEF
- **THEN** the enemy takes 900 damage
