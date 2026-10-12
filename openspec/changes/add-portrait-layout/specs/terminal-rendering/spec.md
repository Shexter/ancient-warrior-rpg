# Spec Delta

## MODIFIED Requirements

### Requirement: Minimum terminal size
The game SHALL require a terminal of at least 80×24 cells, or at least 40×32 cells for the portrait layout, and SHALL show a resize prompt instead of a corrupted layout when the terminal is smaller than both.

#### Scenario: Terminal too small
- **WHEN** the terminal is resized to 60×20 during play
- **THEN** the game shows a message asking for at least 80×24 (or 40×32 on a phone) and resumes rendering when the size is restored

## ADDED Requirements

### Requirement: Portrait layout
When the terminal is narrower than 80 columns and at least 40×32, or `--layout portrait` is given, every screen SHALL draw a 40×32 layout centered in the terminal: the scene (status, sprites, bars, log) in the top rows and the screen's menu as full-width numbered rows in the bottom rows, with Back always the last row. All actions available in the other layouts SHALL be available in portrait.

#### Scenario: Phone-sized terminal
- **WHEN** the game runs in a 45×40 terminal and the Hub is shown
- **THEN** the hero and status are in the top half and Training, Mission, Town, Save and Quit are numbered rows in the bottom half

#### Scenario: Battle on a phone
- **WHEN** a battle is shown in portrait
- **THEN** the enemy, both HP bars and the newest log lines are in the top half, and Attack, Skill, Item, Defend and Flee are rows 1–5 in the bottom half

### Requirement: Tap to select
When the terminal reports mouse or touch clicks, a click on a menu row SHALL select and confirm that row, exactly as its number key would. Keyboard control SHALL keep working unchanged, and mouse reporting SHALL be turned off when the game exits.

#### Scenario: Tapping a menu row
- **WHEN** the player taps the "Town" row on the portrait Hub
- **THEN** the Town opens, as if `3` had been pressed
