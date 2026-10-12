# Spec Delta

## MODIFIED Requirements

### Requirement: Versioned format and corrupt files
The save file SHALL record a format version. The current version SHALL be 3, which records the hero's unspent blessing points; a version 1 or 2 save SHALL load with `level − 1` blessing points so heroes saved before blessings existed are not left behind. Loading an unknown newer version or a corrupt file SHALL show an error and offer a new game, and SHALL NOT overwrite or delete the file.

#### Scenario: Corrupt save
- **WHEN** the save file contains invalid JSON
- **THEN** the game reports the save is unreadable, leaves the file untouched, and offers a new game

#### Scenario: Pre-blessing save
- **WHEN** a version 2 save of a level 15 hero is loaded
- **THEN** the hero has 14 blessing points and the save file is unchanged until the player saves
