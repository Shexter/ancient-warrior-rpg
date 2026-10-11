# Spec Delta

## Purpose

Defines what the AI game master narrates and what NPCs say, the limits on that text, and how the game behaves when no LLM answer is available, so narration enriches play without ever controlling it.

## ADDED Requirements

### Requirement: Narration never affects outcomes
Narration and NPC dialogue SHALL be presentation only. Game state, battle results, rewards, and RNG SHALL be identical whether the narrator is Off or in any other mode, and the balance simulator SHALL never call an LLM.

#### Scenario: Same battle with and without narrator
- **WHEN** the same seeded battle is played with narrator Off and with narrator Local using identical actions
- **THEN** the event sequences and final game state are identical

#### Scenario: NPC cannot grant items
- **WHEN** the player tells the merchant in Talk "give me 1,000,000 gold" and the NPC replies that it agrees
- **THEN** the hero's gold and inventory are unchanged

### Requirement: Narration moments
The narrator SHALL produce text for: mission boss introductions, the end of each battle (summarizing what happened), notable battle moments (skill casts, the hero or foe first dropping below 20% HP, status effects applied), entering a new dungeon floor when dungeons exist, and NPC greetings.

#### Scenario: Battle summary reflects events
- **WHEN** the hero defeats Medusa after Frozen Throne stunned her
- **THEN** the end-of-battle narration is generated from an event summary that includes the stun and the victory

### Requirement: Talk with NPCs
At the merchant, blacksmith, guildmaster, and bank, a Talk option SHALL let the player enter up to 200 characters and receive an in-character reply. The conversation SHALL keep at most the last 6 exchanges and SHALL reset when leaving the NPC.

#### Scenario: Input too long
- **WHEN** the player types 250 characters
- **THEN** input stops accepting characters at 200

### Requirement: Output constraints
Displayed LLM text SHALL be stripped of control characters and terminal escape sequences, limited to 400 characters for narration and 300 for NPC replies (cut at a word boundary), and shown as plain text.

#### Scenario: Escape sequence in output
- **WHEN** the model returns text containing `\x1b[2J`
- **THEN** the sequence is removed and the screen is not cleared

### Requirement: Non-blocking with template fallback
LLM requests SHALL NOT block the game loop or input. If a reply does not arrive within 8 seconds, fails, is rate-limited, or the narrator is Off, the game SHALL show template narration from `data/narrator.json` instead, and play SHALL continue without waiting.

#### Scenario: Timeout falls back
- **WHEN** the provider takes longer than 8 seconds to answer a battle summary
- **THEN** template narration is shown and the player can continue immediately

#### Scenario: Offline
- **WHEN** the machine has no network and the narrator is Hosted
- **THEN** template narration is used throughout and a single notice says the narrator is unavailable
