# Spec Delta

## Purpose

Lets players choose where narration comes from — off, the free hosted tier, their own API key, or a local model — with informed consent, safe key handling, and predictable network behavior.

## ADDED Requirements

### Requirement: Narrator modes and consent
The game SHALL offer narrator modes Off, Hosted, Bring Your Own Key (Gemini, Anthropic, OpenAI-compatible), and Local (OpenAI-compatible base URL, default `http://localhost:11434/v1`). On first run the game SHALL explain what data each mode sends where, and SHALL make no network request until the player picks a mode other than Off. The mode SHALL be changeable from Settings.

#### Scenario: No traffic before consent
- **WHEN** the game starts for the first time and the player has not chosen a mode
- **THEN** no network connection is opened

#### Scenario: Switch to local
- **WHEN** the player selects Local with Ollama running
- **THEN** the next narration request goes to the local server and nothing is sent to the relay

### Requirement: API key handling
A BYOK API key SHALL be read from an environment variable (`GEMINI_API_KEY`, `ANTHROPIC_API_KEY`, `OPENAI_API_KEY`) or from the settings file in the per-user config directory, created with owner-only permissions on POSIX. Keys SHALL never be written to save files, logs, crash output, or the screen beyond the last 4 characters.

#### Scenario: Key masked in settings
- **WHEN** the player opens Settings with a BYOK key configured
- **THEN** the key is shown as `••••` followed by its last 4 characters

#### Scenario: Key not in save
- **WHEN** a game is saved with BYOK mode active
- **THEN** the save file contains no part of the API key

### Requirement: Secure transport
Requests to remote providers and the relay SHALL use HTTPS with certificate verification. Local mode MAY use plain HTTP only for loopback addresses.

#### Scenario: Remote plain HTTP rejected
- **WHEN** the player sets a Local base URL of `http://192.168.1.20:11434/v1`
- **THEN** the setting is rejected with a message that plain HTTP is allowed only for localhost

### Requirement: Client-side pacing
The game SHALL send at most one in-flight narration request at a time and SHALL drop stale requests (for example, a notable-moment request superseded by the battle ending). On a rate-limit response the game SHALL stop requesting for the period the server indicates, defaulting to 60 seconds.

#### Scenario: Rate limited
- **WHEN** the relay responds 429 with `Retry-After: 120`
- **THEN** no further narration requests are sent for 120 seconds and templates are used meanwhile

### Requirement: Clear provider errors
Authentication failures, unreachable local servers, and unknown models SHALL be shown once as a readable message in the narration area with a hint (check key, start Ollama, check model name), and SHALL NOT crash or block play.

#### Scenario: Provider declines to answer
- **WHEN** the provider refuses a request for safety reasons
- **THEN** template narration is shown with no error message

#### Scenario: Bad key
- **WHEN** the BYOK key is rejected by the provider
- **THEN** the player sees one message suggesting they check the key in Settings, and templates are used
