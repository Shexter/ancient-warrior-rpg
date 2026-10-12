# game-content Specification

## Purpose
Defines all game content — hero paths, items, skills, enemies, recipes, missions, and story text — in data files that are validated at startup, so content can change without recompiling.

## Requirements

### Requirement: Data-driven definitions
The game SHALL load hero paths, items, skills, enemies, forge recipes, missions, and story text from JSON files in its data directory at startup. The shipped data SHALL reproduce the numbers in `openspec/changes/archive/2026-10-11-build-core-engine/reference/content-tables.md`, and its names, descriptions, flavor text, title, story lines, and the Avaritia enemy and mission SHALL match `openspec/changes/archive/2026-10-11-reskin-act-one/reference/act-one-names.md`.

#### Scenario: Original starting stats
- **WHEN** a Knight (`fighter`) hero is created with the shipped data
- **THEN** it has 50 max MP, 350 max HP, 50 ATK, 5 DEF, and 0 LIFE

#### Scenario: Content change without rebuild
- **WHEN** an item's ATK value is edited in the data file and the game is restarted
- **THEN** the new value is used without recompiling

#### Scenario: Old save shows new names
- **WHEN** a save written before the re-skin, holding `health_potion` and an equipped `iron_sword`, is loaded
- **THEN** it loads without error and the bag shows "Holy Water" and the status panel shows "Sword"

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
Content entries SHALL be identified by stable lowercase string IDs (for example `iron_sword`), not by numeric category codes. Renaming an entry's display name SHALL NOT change its ID.

#### Scenario: Lookup by ID
- **WHEN** the game looks up `iron_sword`
- **THEN** it returns the Sword definition with 50 ATK

### Requirement: Data location
The game SHALL read content from the directory given by a command-line option, otherwise an environment variable, otherwise the development data directory of the source tree.

#### Scenario: Override data directory
- **WHEN** the game is started with `--data-dir /tmp/mydata`
- **THEN** content is loaded from `/tmp/mydata`

### Requirement: Player-facing vocabulary
The game SHALL be titled from the story data ("Ordo Lucis"), SHALL label the MP resource "Fervor" (abbreviated "FV" where a bar is labeled) everywhere the player sees it, and SHALL use the Act I town names and NPC lines listed in `reference/act-one-names.md`. Internal stat names and save fields SHALL NOT change.

#### Scenario: Fervor in the status panel
- **WHEN** any screen with the status panel is shown
- **THEN** the second bar is labeled "FV" and no "MP" label is visible

#### Scenario: Rejected skill
- **WHEN** the hero tries to cast a skill without enough of the resource
- **THEN** the battle log says "Not enough Fervor." and the turn is kept

#### Scenario: Town names
- **WHEN** the player opens the Town from the Hub
- **THEN** the menu lists Almonry, Forge, Prior, and Thesaurus

### Requirement: Act I mission list
The shipped missions SHALL be, in order, the Act I mission against Avaritia (level 15) and the bonus mission against Medusa (level 20). Avaritia SHALL be a separate enemy that uses Medusa's fight pattern; Medusa's enemy definition SHALL be unchanged.

#### Scenario: Mission order
- **WHEN** a level 20 hero opens Mission Select
- **THEN** the first row is `Act I: "Avaritia"` and the second is `Bonus: "The Gorgon"`, followed by the two "Coming Soon" rows

#### Scenario: Earlier unlock
- **WHEN** a level 15 hero with a weapon and armor views the Hub
- **THEN** Mission is enabled, and below level 15 it is disabled with "Requires Lv 15"

#### Scenario: Medusa unchanged
- **WHEN** the Medusa golden-vector tests run against the shipped data
- **THEN** they pass without any change to the vectors
