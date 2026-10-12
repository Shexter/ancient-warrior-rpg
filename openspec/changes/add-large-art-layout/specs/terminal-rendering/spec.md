# Spec Delta

## ADDED Requirements

### Requirement: Large layout for art screens
When the terminal is at least 120×40 cells and the layout option allows it, the Battle, Hub, Create Hero and Training playback screens SHALL draw a 120×40 layout with large sprites, centered in the terminal. All other screens SHALL keep their 80×24 layout, centered. Below 120×40 every screen SHALL use its 80×24 layout exactly as before. The layout SHALL follow terminal resizes while the game runs.

#### Scenario: Big terminal
- **WHEN** the game runs in a 150×45 terminal and the Battle screen is shown
- **THEN** it is drawn as a 120×40 layout centered in the terminal, with the large hero and enemy sprites

#### Scenario: Shrinking below the threshold
- **WHEN** the terminal is resized from 150×45 to 100×30 during a battle
- **THEN** the battle continues in its 80×24 layout with no change to the battle state

#### Scenario: Non-art screens stay compact
- **WHEN** the Merchant screen is shown in a 150×45 terminal
- **THEN** it is drawn in its 80×24 layout, centered

### Requirement: Glyph-ramp sprites
The game SHALL support a second sprite format where each cell holds one printable ASCII character and one 24-bit color, or is transparent. Drawing it SHALL write the character in its color over the existing background and leave transparent cells untouched. The flash and dim tints SHALL apply to its colors the same way as for half-block sprites. The bake tool SHALL produce this format from a PNG by choosing each cell's character from a brightness ramp and its color from the image.

#### Scenario: Transparent cell
- **WHEN** a glyph sprite with a transparent cell is drawn over a colored background
- **THEN** that cell's character, colors and background are unchanged

#### Scenario: Brightness picks the character
- **WHEN** a PNG with a near-black opaque pixel and a near-white opaque pixel is baked to the glyph format
- **THEN** the dark cell gets a character from the sparse end of the ramp and the bright cell one from the dense end, each colored from its pixel

### Requirement: Art and layout options
The game SHALL accept `--layout auto|compact` (default `auto`) and `--art glyph|blocks` (default `glyph`). `compact` SHALL force the 80×24 layouts at any terminal size. `--art` SHALL choose which large sprite look the large layout uses. Invalid values SHALL print the error and usage and exit with code 2.

#### Scenario: Forced compact
- **WHEN** the game starts with `--layout compact` in a 150×45 terminal
- **THEN** every screen uses its 80×24 layout

#### Scenario: Half-block large art
- **WHEN** the game starts with `--art blocks` in a 150×45 terminal and a battle is shown
- **THEN** the large layout draws the 48×48 half-block hero instead of the glyph hero

### Requirement: Optional large art
Large sprites SHALL be optional per sprite. When a screen's large sprites are missing, that screen SHALL use its 80×24 layout. A large sprite file that exists but is malformed SHALL be reported at startup, naming the file, and the game SHALL exit with code 2 before changing the terminal.

#### Scenario: Missing large enemy
- **WHEN** an enemy has no large sprite in either look and a battle against it is shown in a 150×45 terminal
- **THEN** the Battle screen uses its 80×24 layout

#### Scenario: Broken large sprite
- **WHEN** a large glyph sprite has a row of the wrong length
- **THEN** startup fails with the file name and line, and the terminal is never switched to raw mode
