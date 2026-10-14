# Spec Delta

## MODIFIED Requirements

### Requirement: Scripted enemies and skills
An enemy definition MAY reference a script that decides the enemy's actions each turn. A skill definition SHALL have either data-driven effect fields or a script, not both. Script effects SHALL go through engine operations (attack, damage, heal, halve HP, apply status, extra turn, log message), so they appear in the battle event stream exactly like native effects. The attack operation SHALL take an optional power (a whole number from 0 to 10,000,000, defaulting to the attacker's ATK); with any power it SHALL emit the attack event before the hit, with DEF applied, so a varied or boosted strike still plays as an attack.

#### Scenario: Scripted skill emits events
- **WHEN** the hero with 400 ATK casts a scripted skill that deals `1.5 × ATK` against an enemy with 0 DEF
- **THEN** the battle event stream contains a skill-cast event and a damage event of 600

#### Scenario: Both effect and script
- **WHEN** a skill definition has both effect fields and a script
- **THEN** startup fails with a validation error naming the skill

#### Scenario: Attack with a given power
- **WHEN** an enemy script calls `ctx.attack(250)` against a hero with 0 DEF
- **THEN** the event stream contains the enemy's attack event and an attack damage event of 250

#### Scenario: Attack power must be a whole number
- **WHEN** an enemy script calls `ctx.attack(2.5)`
- **THEN** the script fails, a script-error event is emitted, and the enemy performs a basic attack instead
