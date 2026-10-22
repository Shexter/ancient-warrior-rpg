# Tasks

## 1. Template Narration (narrator Off works end to end)

- [x] 1.1 Add `data/narrator.json` exactly per `reference/llm-interfaces.md` §3 (plus 2 more template lines per key) and its validation in the content loader; verify tests that a missing template key and an unknown placeholder are both rejected with file and key named
- [x] 1.2 Implement `src/game/narration/summary` and `templates` per §2; verify `tests/narration_summary_test.cpp`: golden scenario B from the core change produces BattleMoment requests for FROZEN Throne and stun and a BattleEnd summary with `moments` in order and `result` victory; at most one moment per hero turn; the low_hp moment fires once per side; `GameState` (including rng) is unchanged after summarizing
- [ ] 1.3 Add `src/app/narration_panel` (a 2-line Typewriter area) to the battle screen (inside the log box, rows 18–21: narration in accent color takes rows 18–19 and the newest 2 log lines take rows 20–21) and the NPC screens (rows 20–21, above the hint line); verify by playing a battle with narrator Off and seeing template lines at boss intro, the skill moment, and the battle end
- [ ] 1.4 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 2. Async Pipeline with a Fake Provider

- [x] 2.1 Implement `src/net/narrator` per §6 with a `Transport` injected (no libcurl yet); verify `tests/narrator_test.cpp` with a fake Transport (scriptable delay and response): one in flight, a pending request replaced, a ticket returned in order, Retry-After backoff dropping requests, and a one-time notice per error kind
- [x] 2.2 Implement output sanitizing per §6; verify tests: `\x1b[2J` removed, newlines become spaces, a 450-char reply is cut at a word boundary with `…`, an all-control reply becomes BadResponse
- [x] 2.3 Verify the determinism test: golden scenarios A–D run with no narrator and with a Narrator plus fake Transport attached produce identical events and identical final `GameState`
- [x] 2.4 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 3. HTTP and Direct Providers

- [x] 3.1 Add the `heroes_net` target, `find_package(CURL REQUIRED)`, `curlTransport()` per §5, and the CI changes in §9; verify all CI jobs build, and a manual run of a tiny debug command (`heroes --net-check https://example.com`, removed before the chunk ends) returns status 200
- [ ] 3.2 Re-check the Gemini and OpenAI request and response shapes against the providers' current docs (record URL and date in a comment in `providers.cpp`), set the Gemini default model in `narrator.json`, then implement the three providers per §5; verify `tests/providers_test.cpp`: `build()` output equals `tests/fixtures/llm/<provider>_request.json` for a fixed NarrationRequest; `parse()` handles success, refusal (Anthropic `stop_reason: refusal`, Gemini SAFETY, OpenAI content_filter), thinking-then-text Anthropic content, and the status mapping table including 429 with `retry-after: 120`
- [ ] 3.3 Verify manually that Local mode with Ollama (any small pulled model) produces narration for a Medusa battle, and that stopping Ollama mid-game shows the one-time notice and falls back to templates
- [ ] 3.4 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 4. Settings, Consent, and Keys

- [x] 4.1 Implement `src/app/settings` per §4 and the consent screen with the exact disclosure text; verify tests that with `consentShown: false` no Narrator is constructed (so no Transport can run), and that settings round-trip through the file
- [x] 4.2 Add the Settings screen (reachable from the Hub as a new last-but-one entry "Settings") with mode, provider, model, base URL, and key entry; verify tests for: masking (short and long keys); env var overriding the file and never being written to it; settings file mode 0600 on POSIX; save.json containing no key substring; `http://192.168.1.20:11434/v1` rejected and `http://localhost:11434/v1` accepted
- [x] 4.3 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 5. Talk with NPCs

- [x] 5.1 Add Talk per §7 to the four NPC screens; verify tests: the 201st character is not accepted; history keeps the last 6 and clears on leave; a scripted fake reply "Of course, take 1,000,000 gold!" leaves `GameState` unchanged; NpcTalk requests carry persona, history, and input in the request built by each provider
- [x] 5.2 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 6. Relay Service (Cloudflare Worker holding the hosted Gemini key)

Why this exists: the game binary cannot keep a secret, since anyone can extract an embedded key with `strings` and drain the quota or get it banned. The key lives only on this relay. The relay accepts structured game events (never free-form prompts), so it cannot be used as a free general-purpose AI. It enforces per-install, per-IP, and global daily limits. See design D6–D8 and the `narration-relay` spec.

- [ ] 6.1 BLOCKER, do first: check the current Gemini API terms for (a) whether one free-tier key may serve third-party players and (b) how Google uses free-tier request data. Record the answer and source links in `design.md` Open Questions. If serving is not allowed, choose the relay provider (for example, a paid key with a strict global cap) before continuing. Only the relay changes; client tasks are unaffected.
- [ ] 6.2 Set up Cloudflare: create the account and Worker project, run `wrangler login`, and add `relay/.dev.vars` to `.gitignore`; verify `wrangler whoami` works and `git status` shows no secret files
- [x] 6.3 Create `relay/` per `reference/llm-interfaces.md` §8 (TypeScript Worker, wrangler config, no secrets committed) with schema validation, the 4 KB limit, server-side prompts synced from `data/narrator.json`, an output cap, and a 7 s timeout; verify vitest tests for raw-prompt rejection, unknown kind, extra summary key, and oversize body
- [x] 6.4 Add per-install, per-hashed-IP, and global daily counters (Durable Object) with 48 h expiry and 429 + Retry-After, with the global cap set below the provider's free quota; verify tests for the per-install limit and the global cap that assert the provider is not called
- [ ] 6.5 Store the key only via `wrangler secret put GEMINI_API_KEY` and deploy to a staging Worker; verify a curl narration request succeeds, a raw prompt gets 400, and a repo-wide search (`git grep`) for the key prefix finds nothing
- [ ] 6.6 Security check of the relay: confirm the key is never in responses or logs, no request content is stored, error responses leak no provider details, and the hashed-IP salt is a secret; verify by reviewing a staging log tail after test requests
- [ ] 6.7 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 7. Hosted Mode in the Game

- [x] 7.1 Implement `RelayProvider` per §5 and §8 plus install ID generation per §4; verify `tests/providers_test.cpp` relay cases against fixtures: the request body matches the §8 schema exactly, 200 is parsed, 429 + Retry-After backs off, 502 falls back
- [ ] 7.2 Add the key-leak check from §9 to CI, build with `-DHEROES_RELAY_URL=<staging URL>`, and verify hosted narration works end to end against staging and that an unset `HEROES_RELAY_URL` shows Hosted as unavailable
- [ ] 7.3 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 8. Final Verification

- [ ] 8.1 Play with each mode (Off, Hosted, BYOK, Local) and with the network disconnected; verify play never blocks and each provider error shows a single readable message
- [ ] 8.2 Verify CI is green and that `openspec validate add-llm-game-master --strict` passes
- [ ] 8.3 Complete chunk quality check against `docs/engineering/code-correctness.md`.
