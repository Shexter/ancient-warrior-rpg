# content-scripting Specification

## Purpose
Lets enemy turn behavior and custom skill effects be written as scripts that the engine runs safely and deterministically, so new content needs no recompile.

## Requirements

### Requirement: Scripted enemies and skills
An enemy definition MAY reference a script that decides the enemy's actions each turn. A skill definition SHALL have either data-driven effect fields or a script, not both. Script effects SHALL go through engine operations (attack, damage, heal, halve HP, apply status, extra turn, log message), so they appear in the battle event stream exactly like native effects.

#### Scenario: Scripted skill emits events
- **WHEN** the hero with 400 ATK casts a scripted skill that deals `1.5 × ATK` against an enemy with 0 DEF
- **THEN** the battle event stream contains a skill-cast event and a damage event of 600

#### Scenario: Both effect and script
- **WHEN** a skill definition has both effect fields and a script
- **THEN** startup fails with a validation error naming the skill

### Requirement: Parity with original behavior
The shipped scripted Medusa SHALL produce event sequences identical to the core engine's recorded golden sequences for the same seeds and actions, including the order of random draws.

#### Scenario: Medusa parity
- **WHEN** the core golden battle scenarios A–D are run with Medusa's behavior coming from her script
- **THEN** every event sequence matches the recorded golden sequence on macOS, Linux, and Windows

### Requirement: Sandbox
Scripts SHALL have no access to files, the operating system, process control, module or code loading, the debug library, or global state, and SHALL NOT be able to read or modify game state except through the provided engine operations.

#### Scenario: File access blocked
- **WHEN** a script calls `io.open("save.json")`
- **THEN** the call fails with a script error and no file is opened

#### Scenario: Global write blocked
- **WHEN** a script assigns to a global variable
- **THEN** the call fails with a script error

### Requirement: Determinism
Scripts SHALL obtain randomness only from the engine's seeded RNG and SHALL have no access to wall-clock time or Lua's own random functions. Engine operations SHALL accept only integers.

#### Scenario: Scripted replay
- **WHEN** a battle against a scripted enemy runs twice with seed 42 and identical actions
- **THEN** both runs produce identical event sequences

### Requirement: Runaway and failing scripts
A script that exceeds its instruction budget or memory cap, or raises an error, SHALL be stopped and SHALL leave no partial effects. For an enemy, the failure SHALL be reported as a battle event and the enemy SHALL perform a basic attack instead. For a skill, the action SHALL be rejected without spending MP and it SHALL remain the hero's turn. The game SHALL NOT crash or hang.

#### Scenario: Infinite loop
- **WHEN** an enemy script contains `while true do end`
- **THEN** the script is stopped, a script-error event is emitted, and the enemy performs a basic attack

#### Scenario: Skill script error mid-effect
- **WHEN** a skill script deals damage and then raises an error
- **THEN** the damage is undone, no MP is spent, and the player is told the skill failed

#### Scenario: Syntax error at startup
- **WHEN** a referenced script has a syntax error
- **THEN** startup fails with the script path, line number, and message, like other content validation errors
