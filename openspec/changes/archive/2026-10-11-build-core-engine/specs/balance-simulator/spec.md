# Spec Delta

## Purpose

Runs large numbers of battles headlessly so game balance can be measured and tuned, and so the combat engine's throughput can be benchmarked.

## ADDED Requirements

### Requirement: Batch simulation
A command-line tool SHALL run N mission battles of a described hero build (path, equipped items with upgrade levels, learned skills, optional stat bonuses) against a named enemy, using a named action policy and a base seed. It SHALL report the battle count, seed, threads, win/loss/draw/fled percentages, average hero turns, and the average remaining hero HP across wins.

#### Scenario: Medusa balance check
- **WHEN** the tool runs 100,000 battles of a starting Fighter against Medusa with the always-attack policy
- **THEN** it prints every report field and the win rate is 0.00%

### Requirement: Policies
The tool SHALL support at least two policies: `attack` (always attack) and `smart` (heal with Angel Wings below 30% HP; otherwise cast the strongest affordable, ready damage skill; otherwise attack).

#### Scenario: Unknown policy
- **WHEN** the tool is given `--policy berserk`
- **THEN** it exits with a non-zero code and lists the valid policies

### Requirement: Reproducible results
Battle i SHALL use the seed stream derived from (base seed, i). Results SHALL be identical for the same inputs and base seed regardless of thread count.

#### Scenario: Same seed same report
- **WHEN** the same simulation is run with seed 7, once with 1 thread and once with 8 threads
- **THEN** both reports are byte-identical apart from the threads line

### Requirement: Throughput report
With the benchmark flag the tool SHALL also report total wall time and battles per second.

#### Scenario: Benchmark output
- **WHEN** the tool runs with `--bench`
- **THEN** it prints battles per second and wall time after the balance report

### Requirement: Invalid input
Unknown items, skills, paths, or enemies, and non-positive battle or thread counts, SHALL produce a clear error and a non-zero exit code without running any battles.

#### Scenario: Unknown item
- **WHEN** the tool is given `--weapon iron_swrod`
- **THEN** it exits with an error naming `iron_swrod`
