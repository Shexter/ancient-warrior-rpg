# save-system Specification

## Purpose
Persists a playthrough to a single versioned file and restores it exactly, without ever losing an existing save to a crash or a corrupt file.

## Requirements

### Requirement: Save and load round trip
Saving and then loading SHALL restore the hero, inventory, bank, equipment, learned skills, completed missions, forge level, and RNG state exactly.

#### Scenario: Round trip
- **WHEN** a game is saved and loaded
- **THEN** every persisted field equals its value before saving

### Requirement: Crash-safe writes
Saving SHALL write to a temporary file and atomically replace the previous save, so an interrupted save leaves the previous save intact.

#### Scenario: Interrupted save
- **WHEN** the process is killed during a save
- **THEN** the previous save file still loads

### Requirement: Versioned format and corrupt files
The save file SHALL record a format version. Loading an unknown newer version or a corrupt file SHALL show an error and offer a new game, and SHALL NOT overwrite or delete the file.

#### Scenario: Corrupt save
- **WHEN** the save file contains invalid JSON
- **THEN** the game reports the save is unreadable, leaves the file untouched, and offers a new game

### Requirement: Platform save location
The save SHALL be stored in the platform's per-user data directory, not the working directory.

#### Scenario: Save path on macOS
- **WHEN** the game saves on macOS
- **THEN** the file is under `~/Library/Application Support/`
