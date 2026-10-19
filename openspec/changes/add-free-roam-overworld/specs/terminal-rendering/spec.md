# Spec Delta

## MODIFIED Requirements

### Requirement: Keyboard controls
All screens SHALL be operable with the keyboard alone. The arrow keys (and W/A/S/D) move selection, or the hero on the Overworld, where a held key keeps the hero moving. Enter confirms, Esc goes back, and number keys select menu entries directly; on the Overworld the number keys use abilities instead. During battle animations Enter SHALL skip the current animation.

#### Scenario: Number shortcut
- **WHEN** the player presses `2` on the hub menu
- **THEN** the second menu entry is selected and activated

#### Scenario: Overworld movement keys
- **WHEN** the player presses W on the Overworld
- **THEN** the hero moves one tile up

### Requirement: Large layout for art screens
When the terminal is at least 120×40 cells and the layout option allows it, the Battle, Hub, Create Hero and Training playback screens SHALL draw a 120×40 layout with large sprites, centered in the terminal. The Overworld SHALL instead use the whole terminal, from 80×24 up to 200×60, and centre it in a larger terminal. With `--layout compact` it SHALL use 80×24. All other screens SHALL keep their 80×24 layout, centered. Below 120×40 every screen except the Overworld SHALL use its 80×24 layout exactly as before. The layout SHALL follow terminal resizes while the game runs.

#### Scenario: Big terminal
- **WHEN** the game runs in a 150×45 terminal and the Battle screen is shown
- **THEN** it is drawn as a 120×40 layout centered in the terminal, with the large hero and enemy sprites

#### Scenario: Shrinking below the threshold
- **WHEN** the terminal is resized from 150×45 to 100×30 during a battle
- **THEN** the battle continues in its 80×24 layout with no change to the battle state

#### Scenario: Non-art screens stay compact
- **WHEN** the Merchant screen is shown in a 150×45 terminal
- **THEN** it is drawn in its 80×24 layout, centered

#### Scenario: Overworld fills the terminal
- **WHEN** the Overworld is shown in a 150×45 terminal
- **THEN** it draws a 150×45 view with no margin

#### Scenario: Very large terminal
- **WHEN** the Overworld is shown in a 300×90 terminal
- **THEN** it draws a 200×60 view centered in the terminal

## ADDED Requirements

### Requirement: Key press and release reporting
While the Overworld is shown, the game SHALL ask the terminal to report key presses, repeats and releases if it supports this: the kitty keyboard protocol on macOS and Linux, and native key-up events on Windows. It SHALL return the terminal to its previous keyboard mode when the Overworld closes, when the game quits and when it is interrupted. On a terminal without release reporting, each press and each OS key repeat SHALL count as one movement request, never faster than the walking pace. Every other screen SHALL receive the same keys as before, whichever mode the terminal is in.

#### Scenario: Release stops the hero
- **WHEN** the terminal supports the kitty keyboard protocol and the player releases D while walking
- **THEN** the hero stops at the end of the current tile

#### Scenario: Terminal restored
- **WHEN** the game is interrupted while the Overworld is shown
- **THEN** the terminal's keyboard mode is the same as before the game started

#### Scenario: Plain terminal
- **WHEN** the terminal does not answer the protocol query and the player holds D
- **THEN** the hero takes one step per press or repeat of D, no faster than the walking pace, and stops when the repeats stop
