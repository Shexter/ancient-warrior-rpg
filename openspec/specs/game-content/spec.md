# game-content Specification

## Purpose
Defines all game content — hero paths, items, skills, enemies, recipes, missions, and story text — in data files that are validated at startup, so content can change without recompiling.

## Requirements

### Requirement: Data-driven definitions
The game SHALL load hero paths, items, skills, enemies, forge recipes, missions, and story text from JSON files in its data directory at startup. The shipped data SHALL reproduce the values in this change's `reference/content-tables.md`.

#### Scenario: Original starting stats
- **WHEN** a Fighter hero is created with the shipped data
- **THEN** it has 50 max MP, 350 max HP, 50 ATK, 5 DEF, and 0 LIFE

#### Scenario: Content change without rebuild
- **WHEN** an item's ATK value is edited in the data file and the game is restarted
- **THEN** the new value is used without recompiling

### Requirement: Validation with actionable errors
The game SHALL refuse to start when content is invalid, SHALL report every problem found (not only the first), each naming the file, the entry, and the problem, and SHALL exit with a non-zero code before changing the terminal. Invalid content includes malformed JSON, unknown or missing fields, duplicate or malformed IDs, negative stats or prices, drop rates outside 0–100, references to unknown IDs or missing sprite files, and fields that do not fit the item kind.

#### Scenario: Unknown reference
- **WHEN** a forge recipe references item ID `fire_dustt` that does not exist
- **THEN** startup fails with a message naming `recipes.json`, the recipe, and `fire_dustt`

#### Scenario: Typo in field name
- **WHEN** an item has the field `atack` instead of a known field
- **THEN** startup fails with a message naming the item and the unknown field `atack`

#### Scenario: Malformed JSON
- **WHEN** a data file contains a syntax error
- **THEN** startup fails with the file name and the line and column of the error, and does not crash

### Requirement: Stable string IDs
Content entries SHALL be identified by stable lowercase string IDs (for example `iron_sword`), not by numeric category codes.

#### Scenario: Lookup by ID
- **WHEN** the game looks up `iron_sword`
- **THEN** it returns the Iron Sword definition with 50 ATK

### Requirement: Data location
The game SHALL read content from the directory given by a command-line option, otherwise an environment variable, otherwise the development data directory of the source tree.

#### Scenario: Override data directory
- **WHEN** the game is started with `--data-dir /tmp/mydata`
- **THEN** content is loaded from `/tmp/mydata`
