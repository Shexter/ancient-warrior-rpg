# Spec Delta

## ADDED Requirements

### Requirement: Act I boss balance band
The shipped Avaritia stats SHALL put the Act I mission within reach of a mid-game build and out of reach of a starting one, as measured by the simulator with seed 7, 20,000 battles and the `smart` policy.

#### Scenario: Mid-game build
- **WHEN** each path is simulated against `avaritia` with `--weapon iron_sword:5 --armor steel_shield:5 --skills angel_wings,meteor_fist`
- **THEN** every path's win rate is between 40% and 80%

#### Scenario: Starting build
- **WHEN** each path is simulated against `avaritia` with only its starting weapon and armor and no skills
- **THEN** every path's win rate is below 5%
