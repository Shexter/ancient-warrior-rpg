# Narration (`src/game/narration/`)

The LLM game master is being built (OpenSpec `add-llm-game-master`). So far there's the data, the deterministic offline half, and the narrator's request pipeline with an injected transport; nothing on screen uses it yet, and there's no HTTP library, so nothing talks to a network.

## Data (`data/narrator.json`, `templates.*`)

- `loadNarrator(dataDir)` reads it and `parseNarrator` validates it, reporting every error in the file "narrator.json" with the key as the entry: provider default models (Anthropic `claude-opus-5-5`; Gemini's is left empty until the provider work), limits, the narrator and NPC system prompts, a user prompt per kind, the four town NPCs (name and persona), and offline template lines.
- Every one of the 15 template keys (`BossIntro`, `BattleMoment.skill`, `BattleMoment.low_hp.hero` / `.foe`, `BattleMoment.status`, `BattleEnd.victory` / `.defeat` / `.fled` / `.draw`, `FloorEnter`, `NpcGreeting.<npc>`, `NpcTalk`) must have at least one line; the shipped file has three each. A missing key, an unknown key, or a placeholder other than `{hero} {foe} {location} {detail} {floor} {npcName} {turns}` is an error.
- `fillTemplate(def, key, values, rng)` picks one line with the given RNG (callers use their own stream, never the game's) and fills each placeholder from `values`.

## Battle summaries (`summary.*`)

`summarizeBattleChunk(events, ctx)` turns one player action's battle events into at most one request:
- **BattleEnd** when the battle ended in them: hero, foe, result, turns, the moments so far (at most 5, in order) and the hero's HP percent. It replaces any moment in the same action.
- Otherwise the **first moment** among them, as a BattleMoment (hero, foe, moment, detail, both HP percents): a skill cast (detail: the skill's name), the first time either side falls to 20% HP or less (detail `hero` or `foe`, once per side per battle), or a stun or reflect applied. Later moments in the same action are only remembered for the ending.

`BattleContext` keeps the fighters' names, the HP seen so far, the low-HP flags and the moments. Summarizing reads events only and draws no random numbers.

`tests/narration_summary_test.cpp` checks loading, both validation errors, template filling, golden battle B (Exorcism then Shield of Faith, ending in a victory whose moments start Exorcism, stun, Shield of Faith, reflect), low HP firing once per side, and the ending superseding a moment.

## Narrator pipeline (`src/net/`, library `heroes_net`)

- `Provider` turns a request into an `HttpRequest` and a response back into text; both are pure. `statusError` maps a status the same way for every provider: 401/403 Auth, 404 UnknownModel, 429 RateLimited (its retry-after, default 60 s), 5xx Server, other 4xx BadResponse. `complete` builds, sends through the `Transport` and parses. A transport that gets no response returns Timeout or Unreachable itself.
- `Narrator` runs one worker thread. One request is in flight and one waits; a newer `request()` replaces the waiting one, which never starts. Each request gets a ticket (1, 2, …). `poll()` never blocks and returns replies in order.
- **Backoff:** after a rate limit, requests until its retry-after are dropped at once, each with a RateLimited reply, and nothing is sent.
- **Notices:** `takeNotice()` gives one message the first time each of Auth, Unreachable (naming Ollama when the narrator is `local`) and UnknownModel is seen; refusals and other errors fall back to the template silently.
- **Sanitizing** (in `poll`): control bytes and escape sequences go, newlines and tabs become spaces, runs of spaces collapse, and a reply over the limit (400 characters) is cut at the last space with `…`. A reply with nothing printable left becomes BadResponse.
- `tests/narrator_test.cpp` uses a fake transport the test can hold shut: one in flight with the waiting request replaced, replies in ticket order, backoff sending nothing, each notice once, both Unreachable notices, sanitizing, and golden battle B giving identical events with a Narrator attached.
