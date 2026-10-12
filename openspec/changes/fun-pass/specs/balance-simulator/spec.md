# Spec Delta

## MODIFIED Requirements

### Requirement: Policies
The tool SHALL support at least two policies: `attack` (always attack) and `smart`. The `smart` policy SHALL, in order: defend when the enemy is charging; heal below 30% HP with Angel Wings if affordable, otherwise with the best healing item in stock; cast the strongest affordable, ready damage skill; otherwise attack. The game's win-chance estimates SHALL use the same `smart` policy.

#### Scenario: Unknown policy
- **WHEN** the tool is given `--policy berserk`
- **THEN** it exits with a non-zero code and lists the valid policies

#### Scenario: Smart defends a charge
- **WHEN** the enemy has the Charging status on the hero's turn
- **THEN** the `smart` policy chooses Defend

### Requirement: Act I boss balance band
Each Act I boss SHALL be within reach of its intended build only when the player plays well, as measured by the simulator with seed 7, 20,000 battles per path: with the build in `reference/fun-pass-tables.md`, every path's `smart` win rate SHALL be between 50% and 80%, and its `attack` win rate SHALL be at least 30 percentage points lower. With only starting gear, every path's `smart` win rate SHALL be below 5%.

#### Scenario: Mid-game build
- **WHEN** each path is simulated against `avaritia`, `gula` and `ira` with that boss's intended build
- **THEN** `smart` wins 50–80% and `attack` wins at least 30 points less, for every path and boss

#### Scenario: Starting build
- **WHEN** each path is simulated against each Act I boss with only its starting weapon and armor and no skills
- **THEN** every `smart` win rate is below 5%
