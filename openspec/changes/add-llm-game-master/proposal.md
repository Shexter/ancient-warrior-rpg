# Proposal

## Why

The original game tells its story through fixed, typed-out text, and every battle reads the same. An LLM narrator can describe each battle and voice the NPCs from what actually happened, so no two runs read alike. The deterministic engine stays the only authority over outcomes. Players should get this with no setup (a free hosted tier), with their own API key, or fully offline with a local model, and the game must remain fully playable with narration off.

## What Changes

- An AI narrator that turns battle and dungeon event batches into short narration, plus boss intros and NPC flavor lines.
- "Talk" option at town NPCs: the player types a line and the NPC answers in character. Dialogue never changes game state.
- Three narrator modes chosen on first run and changeable in settings:
  - **Hosted (free):** through a project-run relay holding the author's Gemini key, rate-limited per install.
  - **Bring your own key:** direct calls to Gemini, Anthropic Claude, or any OpenAI-compatible API.
  - **Local:** an OpenAI-compatible server on the player's machine (Ollama, LM Studio, llama.cpp server).
  - plus **Off**, the default until the player chooses.
- A small relay service (separate deployable) that holds the hosted key, builds prompts server-side, and enforces rate and size limits.
- Template narration used whenever the LLM is off, slow, rate-limited, or failing.

## Capabilities

### New Capabilities
- `llm-narration`: what the narrator and NPCs say, when, how output is constrained, and fallback behavior.
- `llm-providers`: narrator modes, provider configuration, key handling, consent, and network behavior in the game client.
- `narration-relay`: the hosted relay that serves the free tier without exposing the API key.

### Modified Capabilities
- None. Game rules and outcomes are unchanged; narration only consumes existing events. Depends on `build-core-engine`; uses dungeon events if `add-procedural-dungeons` has shipped.

## Impact

- New HTTP/TLS client dependency in the game (libcurl).
- New `src/game/narration/` (event summarizing, templates), `src/net/` (providers), settings screen, and `data/narrator.json` (tone, personas, templates).
- New `relay/` directory: a serverless function (TypeScript, Cloudflare Workers) with its own tests and deploy config. The Gemini key exists only as a relay secret.
- Privacy: in Hosted, BYOK, and Local modes, game events, the hero name, and Talk input leave the game process. Shown to the player before first use.
