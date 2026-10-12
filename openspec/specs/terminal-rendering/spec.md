# terminal-rendering Specification

## Purpose
Draws the game to an ordinary terminal as colored pixel-art sprites and text, reads keyboard input without blocking, and paces frames so animation is smooth on macOS, Linux, and Windows.

## Requirements

### Requirement: Cross-platform terminal output
The game SHALL run in a standard terminal on macOS, Linux, and Windows 10+ using only ANSI/VT escape sequences, and SHALL restore the terminal (cursor, echo, line mode, colors, alternate screen) on normal exit, on Ctrl+C, and on a fatal error.

#### Scenario: Terminal restored after quit
- **WHEN** the player quits from any screen
- **THEN** the shell prompt is usable, input echoes, and the cursor is visible

#### Scenario: Terminal restored after interrupt
- **WHEN** the player presses Ctrl+C during a battle animation
- **THEN** the program exits and the terminal is restored to its prior state

### Requirement: Diff rendering
The renderer SHALL write only the cells that changed since the previously presented frame.

#### Scenario: Unchanged frame writes nothing
- **WHEN** two consecutive frames are identical
- **THEN** presenting the second frame writes zero cell updates to the terminal

#### Scenario: Single changed cell
- **WHEN** exactly one cell differs from the previous frame
- **THEN** presenting the frame writes an update for that cell only

### Requirement: Half-block sprites
The renderer SHALL draw sprites at two vertical pixels per terminal cell using 24-bit color, and SHALL treat transparent sprite pixels as not drawn so the background shows through.

#### Scenario: Transparent pixel preserves background
- **WHEN** a sprite with a transparent top-half pixel is drawn over a colored background
- **THEN** the cell's upper half keeps the background color

### Requirement: Minimum terminal size
The game SHALL require a terminal of at least 80×24 cells and SHALL show a resize prompt instead of a corrupted layout when the terminal is smaller.

#### Scenario: Terminal too small
- **WHEN** the terminal is resized to 60×20 during play
- **THEN** the game shows a message asking for at least 80×24 and resumes rendering when the size is restored

### Requirement: Keyboard controls
All screens SHALL be operable with the keyboard alone: arrow keys (and W/A/S/D) move selection, Enter confirms, Esc goes back, and number keys select menu entries directly. During battle animations Enter SHALL skip the current animation.

#### Scenario: Number shortcut
- **WHEN** the player presses `2` on the hub menu
- **THEN** the second menu entry is selected and activated

### Requirement: Color fallback
The renderer SHALL use 24-bit color when the terminal reports truecolor support and SHALL otherwise map colors to the 256-color palette.

#### Scenario: 256-color terminal
- **WHEN** the game runs in a terminal without `COLORTERM=truecolor`
- **THEN** sprites render using 256-color escape codes

### Requirement: Non-blocking input and fixed timestep
The game SHALL read keys without blocking the loop and SHALL advance animations on a fixed timestep independent of rendering speed.

#### Scenario: Animation continues without input
- **WHEN** no key is pressed for two seconds during a battle animation
- **THEN** the animation keeps playing and completes in the same simulated time as on a faster machine
