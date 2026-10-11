# Spec Delta

## Purpose

Serves the free hosted narrator tier: accepts structured game events from players' games, calls the LLM with a key that never leaves the server, and limits usage so the free quota cannot be exhausted or abused.

## ADDED Requirements

### Requirement: Key confidentiality
The provider API key SHALL exist only as a relay secret. It SHALL NOT appear in the game binary, repository, responses, or relay logs.

#### Scenario: Binary contains no key
- **WHEN** the release binary is searched for the key's prefix
- **THEN** no match is found

### Requirement: Structured requests only
The relay SHALL accept only structured narration requests (kind, compact event summary, NPC ID, and up to 200 characters of Talk input), SHALL build the prompt itself from server-side templates, and SHALL reject free-form prompts, unknown kinds, and request bodies over 4 KB.

#### Scenario: Raw prompt rejected
- **WHEN** a client posts `{"prompt": "write my essay"}`
- **THEN** the relay responds 400 and makes no LLM call

### Requirement: Rate limits
The relay SHALL limit requests per install ID and per client IP (per-minute and per-day), and SHALL enforce a global daily cap below the provider's free quota. When a limit is hit it SHALL respond 429 with a `Retry-After` header.

#### Scenario: Per-install limit
- **WHEN** one install ID exceeds its per-minute limit
- **THEN** further requests from it receive 429 with `Retry-After` until the window resets

#### Scenario: Global cap
- **WHEN** the global daily cap is reached
- **THEN** all clients receive 429 until the next day, and the provider is not called

### Requirement: Bounded output and failures
The relay SHALL cap model output length, SHALL time out provider calls after 7 seconds, and SHALL return a plain error status without provider details on failure.

#### Scenario: Provider outage
- **WHEN** the provider returns 500
- **THEN** the relay responds 502 with a generic message and the game falls back to templates

### Requirement: Minimal data retention
The relay SHALL NOT store request content. It SHALL keep only counters needed for rate limiting, keyed by install ID and a hashed IP, expiring within 48 hours.

#### Scenario: No content stored
- **WHEN** a Talk request is processed
- **THEN** the relay's storage contains only rate-limit counters afterward
