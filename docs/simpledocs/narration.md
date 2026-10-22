# Narration (`src/game/narration/`)

The LLM game master is being built (OpenSpec `add-llm-game-master`). So far there's the data, the deterministic offline half, and the narrator's request pipeline with an injected transport; nothing on screen uses it yet, and there's no HTTP library, so nothing talks to a network.

## Data (`data/narrator.json`, `templates.*`)

- `loadNarrator(dataDir)` reads it and `parseNarrator` validates it, reporting every error in the file "narrator.json" with the key as the entry: provider default models (Anthropic `claude-opus-5-5`, Gemini `gemini-3.5-flash-lite`), limits, the narrator and NPC system prompts, a user prompt per kind, the four town NPCs (name and persona), and offline template lines.
- Every one of the 15 template keys (`BossIntro`, `BattleMoment.skill`, `BattleMoment.low_hp.hero` / `.foe`, `BattleMoment.status`, `BattleEnd.victory` / `.defeat` / `.fled` / `.draw`, `FloorEnter`, `NpcGreeting.<npc>`, `NpcTalk`) must have at least one line; the shipped file has three each. A missing key, an unknown key, or a placeholder other than `{hero} {foe} {location} {detail} {floor} {npcName} {turns}` is an error.
- `fillTemplate(def, key, values, rng)` picks one line with the given RNG (callers use their own stream, never the game's) and fills each placeholder from `values`.
- `narrateOffline(def, request, rng)` is the built-in line for a request: the template key from its kind (`BattleMoment.<moment>`, `.low_hp.<side>`, `BattleEnd.<result>`, `NpcGreeting.<npc>`), with the summary's values (and the NPC's name) filled in.
- Startup loads `narrator.json` after the dungeon (an error stops startup with exit 2) and puts it on the Session.

## Battle summaries (`summary.*`)

`summarizeBattleChunk(events, ctx)` turns one player action's battle events into at most one request:
- **BattleEnd** when the battle ended in them: hero, foe, result, turns, the moments so far (at most 5, in order) and the hero's HP percent. It replaces any moment in the same action.
- Otherwise the **first moment** among them, as a BattleMoment (hero, foe, moment, detail, both HP percents): a skill cast (detail: the skill's name), the first time either side falls to 20% HP or less (detail `hero` or `foe`, once per side per battle), or a stun or reflect applied. Later moments in the same action are only remembered for the ending.

`BattleContext` keeps the fighters' names, the HP seen so far, the low-HP flags and the moments. Summarizing reads events only and draws no random numbers.

`tests/narration_summary_test.cpp` checks loading, both validation errors, template filling, golden battle B (Exorcism then Shield of Faith, ending in a victory whose moments start Exorcism, stun, Shield of Faith, reflect), low HP firing once per side, and the ending superseding a moment.

## Narrator pipeline (`src/net/`, library `heroes_net`)

- `Provider` turns a request into an `HttpRequest` and a response back into text; both are pure. `statusError` maps a status the same way for every provider: 401/403 Auth, 404 UnknownModel, 429 RateLimited (its retry-after, default 60 s), 5xx Server, other 4xx BadResponse. `complete` builds, sends through the `Transport` and parses. A transport that gets no response returns Timeout or Unreachable itself.
- `Narrator` runs one worker thread (a plain `std::thread` with a stop flag; Xcode 16's libc++ has no `std::jthread`). One request is in flight and one waits; a newer `request()` replaces the waiting one, which never starts. Each request gets a ticket (1, 2, …). `poll()` never blocks and returns replies in order.
- **Backoff:** after a rate limit, requests until its retry-after are dropped at once, each with a RateLimited reply, and nothing is sent.
- **Notices:** `takeNotice()` gives one message the first time each of Auth, Unreachable (naming Ollama when the narrator is `local`) and UnknownModel is seen; refusals and other errors fall back to the template silently.
- **Sanitizing** (in `poll`): control bytes and escape sequences go, newlines and tabs become spaces, runs of spaces collapse, and a reply over the limit (400 characters) is cut at the last space with `…`. A reply with nothing printable left becomes BadResponse.
- `tests/narrator_test.cpp` uses a fake transport the test can hold shut: one in flight with the waiting request replaced, replies in ticket order, backoff sending nothing, each notice once, both Unreachable notices, sanitizing, and golden battle B giving identical events with a Narrator attached.

## Settings (`src/app/settings.*`)

- `settings.json` sits beside the save (the same `HEROES_SAVE_DIR` override) and never goes into it: `version`, `consentShown`, and the narrator's `mode` (off, hosted, byok, local), `provider` (gemini, anthropic, openai), `model`, `baseUrl`, `apiKey` and `installId`. A missing or damaged file gives the defaults: consent not shown, Off.
- `saveSettings` writes a temporary file, makes it readable only by the owner on POSIX, then renames it over the old one.
- `mask(key)` shows `••••` and the last 4 characters (just `••••` under 8). `resolveKey` takes the provider's environment variable (`GEMINI_API_KEY`, `ANTHROPIC_API_KEY`, `OPENAI_API_KEY`) over the file's key, and the variable is never written to the file.
- `validBaseUrl` wants `http(s)://host[:port]/path`, with plain http only for `localhost`, `127.0.0.1` or `::1`. `newInstallId` is 32 lowercase hex characters from `std::random_device`.
- `kConsentText` is the first-run disclosure. Nothing shows it yet, and nothing builds a narrator from the settings until real providers exist.
- `tests/settings_test.cpp` covers the defaults, a round trip with the 0600 mode, a damaged file, masking, the env var winning and never being written, a save holding no key, the URL rule, and install ids.

## Providers (`src/net/providers.*`)

Each provider builds an HTTP request from a narration request and reads the reply; both are pure, and nothing here sends anything (there's no HTTP library yet).
- **Messages:** the system prompt is the narrator prompt, or for an NPC the NPC prompt with its name and persona filled in. The user text is the kind's prompt with `{summary}` (compact JSON) and `{input}` filled in. A talk's history goes first as alternating turns, the player's wrapped in `<player>` tags.
- **Anthropic:** `POST https://api.anthropic.com/v1/messages` with `x-api-key`, `anthropic-version: 2023-06-01` and the fallback beta header; `max_tokens` 2048, low effort. A `refusal` stop reason is Refused; otherwise the text blocks are joined, skipping any thinking blocks.
- **Gemini:** `POST …/v1beta/models/<model>:generateContent` with `x-goog-api-key`; the history's replies have the role `model`. A prompt block reason or a SAFETY finish is Refused; otherwise the first candidate's parts are joined.
- **OpenAI-compatible:** `POST <base>/chat/completions` (OpenAI's base by default, `http://localhost:11434/v1` for Local), `authorization: Bearer <key>` only when there is a key. OpenAI gets `max_completion_tokens` and Local gets `max_tokens`. A `content_filter` finish is Refused.
- An empty reply, malformed JSON, or a field of the wrong type is BadResponse. The timeout comes from `narrator.json`.
- `tests/providers_test.cpp` compares each built request with `tests/fixtures/llm/<provider>_request.json` (written by hand from the documented shapes), and checks talk requests, each kind of refusal, Anthropic's thinking-then-text replies, and the status mapping.

## In battle (narrator Off)

With a narrator on the Session (the real game always has one), the battle screen narrates with built-in lines, drawing from `Rng::stream(seed, 0xA11CE)`, never the game's RNG:
- A mission opens with the boss's introduction (`{foe}`, `{location}`).
- After each action, `summarizeBattleChunk` gives at most one request. Its line waits until the events before it have played, then types itself out in the accent colour on the log box's top two rows (the log keeps the rest). A new line replaces the old.
- At the end, the screen stays until the ending line has typed out and a further second has passed, then moves on as before.
- Without a narrator (most screen tests) nothing changes. `BattleScreen.WithTheNarratorOffBuiltInLinesNarrateTheFight` checks the intro and an ending line in a Medusa fight.

## NPC greetings (narrator Off)

With a narrator on the Session, the Almonry (merchant), the Forge (blacksmith), the Prior (guildmaster) and the Thesaurus (bank) each greet the hero on entry with one of their built-in lines, typed out in the accent colour on rows 20–21, above the hint. The line is picked from `Rng::stream(seed ^ gold, 0xA11CE)`, so visits vary without touching the game's RNG. The phone layout shows no greeting. `Town.EachNpcGreetsTheHeroWithItsOwnLine` checks three of them, and that nothing shows without a narrator.
