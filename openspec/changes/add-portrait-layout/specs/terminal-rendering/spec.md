# Spec Delta

## MODIFIED Requirements

### Requirement: Minimum terminal size
The game SHALL require a terminal of at least 80×24 cells, or at least 48×44 cells for the portrait layout, and SHALL show a resize prompt instead of a corrupted layout when the terminal is smaller than both.

#### Scenario: Terminal too small
- **WHEN** the terminal is resized to 60×20 during play
- **THEN** the game shows a message asking for at least 80×24 (or 48×44 on a phone), wrapped to fit the terminal's width and resumes rendering when the size is restored

## ADDED Requirements

### Requirement: Portrait layout
When the terminal is narrower than 80 columns and at least 48×44, or `--layout portrait` is given, every screen SHALL draw a 48×44 layout centered in the terminal, top to bottom: a header (title, a ✕ Back button, key stats), the scene (sprites, bars, log), the screen's menu as full-width boxed numbered rows, and a control pad of boxed ▲ ◀ ● ▶ ▼ buttons that act as the arrow keys and Enter. All actions available in the other layouts SHALL be available in portrait.

#### Scenario: Phone-sized terminal
- **WHEN** the game runs in a 48×47 terminal and the Hub is shown
- **THEN** the header shows the hero's level, HP and gold, Training, Mission, Town, Save and Quit are boxed rows below the hero, and the control pad is at the bottom

#### Scenario: Battle on a phone
- **WHEN** a battle is shown in portrait
- **THEN** both HP bars are in the header, the enemy and the newest log lines are in the scene, and Attack, Skill, Item, Defend and Flee are boxed rows 1–5 above the control pad

#### Scenario: Control pad
- **WHEN** the player taps ▼ and then ● on the portrait Hub
- **THEN** the selection moves to Mission and it opens, as if ↓ and Enter had been pressed

#### Scenario: Back button
- **WHEN** the player taps ✕ in a town screen's header
- **THEN** the screen goes back, as if Esc had been pressed

### Requirement: Tap to select
When the terminal reports mouse or touch clicks, a click on a menu row SHALL select and confirm that row, exactly as its number key would. Keyboard control SHALL keep working unchanged, and mouse reporting SHALL be turned off when the game exits.

#### Scenario: Tapping a menu row
- **WHEN** the player taps the "Town" row on the portrait Hub
- **THEN** the Town opens, as if `3` had been pressed
