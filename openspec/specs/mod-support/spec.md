# mod-support Specification

## Purpose
Lets players drop folders of content and scripts into a mods directory to add new items, enemies, skills, and missions, or override existing ones, without rebuilding the game.

## Requirements

### Requirement: Mod discovery and order
The game SHALL load each subfolder of `mods/` that contains a `mod.json` manifest (name, version, optional load priority) after the base content, in ascending priority, ties broken by folder name.

#### Scenario: Mod adds enemy
- **WHEN** a mod defines a new enemy `cyclops`
- **THEN** `cyclops` can be used by missions, including missions defined by the same mod

### Requirement: Overrides by ID
A mod entry with the same ID as an existing entry SHALL replace it, and the game SHALL list every override at startup in a log line naming the mod and ID.

#### Scenario: Two mods override same item
- **WHEN** mods A (priority 1) and B (priority 2) both override `iron_sword`
- **THEN** B's definition is used and the startup log reports both overrides

### Requirement: Mod validation and isolation
Mod content and scripts SHALL pass the same validation and sandbox rules as base content. A mod that fails validation SHALL be skipped as a whole with a reported reason, and the game SHALL start with the remaining content.

#### Scenario: Broken mod skipped
- **WHEN** a mod references an unknown item ID
- **THEN** that mod is skipped with the reason shown, and the game starts with base content and other mods

### Requirement: Saves record mods
A save SHALL record the names and versions of the active mods. Loading a save whose mods are missing SHALL warn the player before continuing and SHALL NOT delete the file.

#### Scenario: Missing mod on load
- **WHEN** a save made with mod `cyclops-pack` is loaded without that mod installed
- **THEN** the game warns that `cyclops-pack` is missing and asks whether to continue
