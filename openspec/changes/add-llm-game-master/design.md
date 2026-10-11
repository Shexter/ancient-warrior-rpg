# Design

## Context

This change builds on `build-core-engine`: every battle and town action already returns an ordered `std::vector<Event>`, and the UI animates those events. Narration is a second consumer of the same events. The author wants a zero-setup free tier backed by their Gemini key. A client application cannot keep a secret, so the hosted tier needs a server-side relay.

The implementation detail lives in `reference/llm-interfaces.md`: source layout, summary schemas, `narrator.json` content including prompts and personas, settings file, provider wire formats, worker contract, relay API and limits, and CI changes.

| chunk | read |
|---|---|
| 1 Templates | §1–§3 |
| 2 Async pipeline | §5 (interfaces only), §6 |
| 3 Providers | §5 |
| 4 Settings and consent | §4 |
| 5 Talk | §3 (npcs, prompts), §7 |
| 6 Relay | §8 |
| 7 Hosted | §5 (Relay), §8 (request) |

Chunks 4 and 6 (keys, consent, relay security) get a closer review before approval.

## Goals / Non-Goals

**Goals:**
- The game is fully playable with no network and no keys, at the same quality minus AI text.
- The provider is swappable behind one interface, and every provider is testable without the network.
- The free tier cannot leak its key or be repurposed as a general-purpose LLM proxy.

**Non-Goals:**
- An LLM choosing actions, generating quests, items, or stats, or changing any game state.
- Streaming (token-by-token) responses. The UI typewrites the complete reply instead.
- OS keychain integration, in-process model inference, voice.
- Accounts or payment for the hosted tier.

## Decisions

### D1. Narrator is a pure event consumer
`Narrator::onEvents(span<const Event>)` builds a compact summary such as `{"kind":"battle_end","hero":"Aria","foe":"Medusa","turns":9,"moments":["stun","low_hp"],"result":"victory"}`. It queues one request and returns immediately. The battle code knows nothing about narration. Determinism is guaranteed structurally, and a test plays the same seeded battle with the narrator Off and with a fake provider and compares the results.

### D2. Templates first, LLM second
Every narration moment has template lines in `data/narrator.json`, picked by a narration-only RNG that is separate from the game RNG. The template shows immediately. When an LLM reply arrives within 8 s and the moment is still current, it replaces the template with a typewriter effect. That way latency never shows up as an empty screen.

### D3. One worker thread and a single-slot queue
A `std::jthread` runs the HTTP calls. The queue holds at most one pending request, and a newer request replaces an unsent one. Replies return through a mutex-protected slot that the game loop polls once per frame. There is no async framework.
*Alternative:* `std::async` per request. It can't cancel stale work or enforce one in-flight request.

### D4. libcurl for HTTP
libcurl comes with the OS on macOS and is a standard package on Linux, and vcpkg provides it on the Windows CI runner. It is found with `find_package(CURL)`. Certificate verification is on by default and is never disabled.
*Alternatives:* cpp-httplib with OpenSSL (OpenSSL setup on Windows is harder); Boost.Beast (a large dependency for a few POST requests).

### D5. Provider interface
`Provider::complete(const NarrationRequest&) -> std::expected<std::string, ProviderError>` with four implementations:
- `RelayProvider`: posts the structured request. The relay builds the prompt.
- `GeminiProvider`, `AnthropicProvider`, `OpenAICompatProvider`: build the prompt locally from the same templates as the relay, which are copied to both sides from `data/narrator.json` at build or deploy time. Local mode reuses `OpenAICompatProvider` with a loopback base URL.

Request bodies for each provider are covered by golden-JSON tests. Model names live in settings, with defaults in `data/narrator.json`, so a model rename is a data change.

Providers split `build()` and `parse()` into pure functions, with the network injected as a `Transport`. All provider tests run offline against fixtures, and libcurl appears in exactly one function.

The Anthropic request shape was checked against the Claude API reference:
- Default model `claude-opus-5-5` at `effort: "low"`, with server-side `fallbacks: "default"`.
- Text is read only from `type == "text"` content blocks, after checking for `stop_reason == "refusal"`.
- No sampling parameters (rejected by current models).

The model is a player setting. A cheaper model such as `claude-haiku-5-5` is the player's choice to make in Settings.

Exact request and response shapes for all four providers are in `reference/llm-interfaces.md` §5.

### D6. Relay on Cloudflare Workers
A single TypeScript Worker in `relay/` exposes `POST /v1/narrate`. It validates the schema and size, builds the prompt from server-side templates, calls Gemini with the secret key, caps output tokens, and times out at 7 s. Rate limiting uses a Durable Object counter keyed by install ID and by SHA-256 of the IP with a server salt, plus a global daily counter. The free plan covers the expected volume, and there is no server to maintain.
*Alternatives:* a VPS running a small server (needs patching and uptime care); Firebase or Vercel functions (would also work, but Workers' built-in Durable Objects make the counters simplest).

### D7. Anonymous install ID
On first choosing Hosted, the game generates a random 128-bit ID and stores it in settings. It identifies the install only for rate limiting and is not tied to any person. Spoofing install IDs is countered by the per-IP limit and the global cap.

### D8. Prompt-injection posture
Talk input and the hero name are untrusted, but the model's output is only displayed: it never executes and is never parsed into state. Prompts wrap player text in delimiters with an instruction to stay in character. The real protection is structural (D1), plus output sanitizing (control and escape stripping, length caps) before rendering.

## Risks / Trade-offs

- [Free quota exhausted by popularity or abuse] → Per-install, per-IP, and global caps, with templates as the fallback. Worst case, the hosted tier is down for the day and the game still works.
- [Provider API or model names change] → Model names are in data, and providers are isolated behind golden tests. The relay can be redeployed independently of game releases.
- [Small local models write poor or off-tone text] → Length caps, a persona in the system prompt, and templates when the output is empty after sanitizing.
- [Latency makes narration feel late] → The template-first approach (D2). Only the battle summary and NPC replies wait, and only up to 8 s.
- [The relay becomes a cost or abuse target] → No free-form prompts (structured kinds only), a 4 KB body limit, and output token caps.

## Open Questions

- **Must be resolved before the relay is deployed (task 6.1):** Do the Gemini API free-tier terms allow serving third-party players from one key, and how does Google use free-tier request data? If serving is not allowed, the relay's provider changes (for example, a paid key with a strict global cap). The client is unaffected because it only talks to the relay. Whatever the answer, the first-run disclosure (`llm-providers` spec) must state how Hosted-mode data is used.
