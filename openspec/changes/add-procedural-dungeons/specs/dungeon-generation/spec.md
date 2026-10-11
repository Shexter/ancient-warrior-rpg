# Spec Delta

## Purpose

Generates multi-floor dungeon layouts with rooms, corridors, stairs, enemies, and chests from a seed, so every expedition is different yet any one can be reproduced exactly.

## ADDED Requirements

### Requirement: Seeded reproducible floors
Given the same seed and floor number, generation SHALL produce an identical floor (tiles, stairs, enemies, chests) on every supported platform.

#### Scenario: Same seed same floor
- **WHEN** floor 3 is generated twice with seed 1234
- **THEN** both floors are identical tile-for-tile and entity-for-entity

### Requirement: Connectivity
Every walkable tile on a floor SHALL be reachable from the entry point, and every floor except the last SHALL contain exactly one down-staircase reachable from the entry.

#### Scenario: Connectivity across many seeds
- **WHEN** 10,000 floors are generated with consecutive seeds
- **THEN** every floor passes the reachability check

### Requirement: Depth scaling
Enemy selection and strength SHALL scale with floor depth using the enemy pools and multipliers in dungeon data, and SHALL draw only from enemies defined in game content.

#### Scenario: Deeper floors are harder
- **WHEN** floors 1 and 5 are generated with the same seed
- **THEN** the average enemy HP on floor 5 is higher than on floor 1

### Requirement: Placement rules
Enemies and chests SHALL NOT be placed on the entry tile, on stairs, or inside walls, and no enemy SHALL be placed within 3 tiles of the entry.

#### Scenario: Safe start
- **WHEN** any floor is generated
- **THEN** no enemy occupies a tile within 3 steps of the entry point

### Requirement: Generation performance
Generating an 80×40 floor SHALL take under 5 ms in a release build on the CI Linux runner.

#### Scenario: Generation benchmark
- **WHEN** the benchmark generates 1,000 floors
- **THEN** the mean generation time is under 5 ms per floor
