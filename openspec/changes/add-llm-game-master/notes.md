# add-llm-game-master notes

## 2026-10-22: tasks 1.1 and 1.2, run without a stop for approval (the user's /goal)

SPEC DRIFT:
- **`narrator.json` has its own loader** (`loadNarrator`, like the dungeon's and the world's) rather than joining `loadContent`. Nothing loads it at startup yet; the panel (1.3) will.
- **Golden scenario B:** after the reskin, frozen_throne is "Exorcism" and golden_bell is "Shield of Faith". Turn 1 casts Exorcism and stuns, so with "one moment request per hero turn" the stun is remembered for the ending, not requested; the test checks that (task 1.2's wording, "requests for FROZEN Throne and stun", predates the per-turn rule as now applied).
- "At most one per hero turn" is applied per player action (one `Battle::act`), which covers the hero's turn and the enemy's reply.
- Gemini's default model is left empty until task 3.2 checks Google's current docs.

Not done here, and why:
- **1.3 (the narration panel)** changes the battle screen's log rows and the NPC screens, and ends with "verify by playing", which needs a person.
- **Chunks 3, 6 and 7** need libcurl in CI, real provider docs and keys, a Cloudflare account, `wrangler login`, secrets and a staging deploy: accounts and keys are the user's to create and enter. 6.1 (Gemini terms) is a blocker the user should decide.

Verification: clang debug, asan (UBSan clean), ci and GCC 14 Release `-Werror` 639/639; window-off 604/604; mingw syntax check clean.

## Chunk 2: the narrator pipeline with a fake transport

SPEC DRIFT:
- **`Transport` returns a `ProviderError`** on failure instead of an error string, so the transport says whether it was a timeout or an unreachable host (the reference maps those two from libcurl anyway).
- **`Narrator` takes an `Options` struct** (the character limit from `narrator.json`, and whether it's Local, which changes the Unreachable notice).
- **Notices are read with `takeNotice()`**, once each.
- The determinism check (2.3) runs golden battle B with and without a Narrator attached. Scenarios A, C and D drive the same `Battle` with the same summarizer, so B stands for all four; the golden tests themselves still pin A–D.
- Narrator tests were repeated 20 times under asan and GCC 14 without a failure. There's no ThreadSanitizer preset, so data races are checked by design (one mutex for all shared state), not by a tool.

Verification: clang debug, asan (UBSan clean), ci and GCC 14 Release `-Werror` 645/645; window-off 610/610; mingw syntax check clean (jthread is available there).

## Part of chunk 4: settings

- `src/app/settings.*` is built and tested: load/save (temp then rename, mode 0600 on POSIX), masking, env-var keys winning and never written, base URL validation, install ids, and the consent text. 4.2's masking, env-var, 0600, no-key-in-save and URL tests are among them.
- **4.1 and 4.2 stay unticked:** the consent screen, the Settings screen and the "no Narrator without consent" check need a way to build a Narrator from settings, which needs real providers (3.2, after re-checking the providers' docs) and libcurl (3.1).
- The window's font atlas gained `•` (U+2022), which the masked key uses.

## Task 3.2: the three direct providers (pure build and parse)

- Shapes rechecked on 2026-10-22: Gemini from https://ai.google.dev/api/generate-content and the models page (default model `gemini-3.5-flash-lite`, the docs' fastest, cheapest stable Flash-Lite). OpenAI's API reference needs a sign-in, so it couldn't be read; search results quote it marking `max_tokens` deprecated in favour of `max_completion_tokens`. Anthropic follows the reference as written. The sources are recorded in `providers.cpp`.
- SPEC DRIFT: OpenAI requests send `max_completion_tokens` while Local keeps `max_tokens` (Ollama-style servers), against the reference's `max_tokens` for both.
- **3.2 stays unticked** until the request shapes have been seen working against the real services, which needs 3.1 (libcurl, CI) and keys; the fixtures are hand-written from the docs.

## Task 1.3, battle part

- The battle screen narrates with built-in lines (boss intro, moments, the ending) in the log box's top two rows, as the task describes. The ending line holds the screen for a second after it has typed out, otherwise the result screen replaced it in the same frame.
- The NPC screens greet on rows 20–21 above the hint (not on the phone layout).
- **Still to do:** the task's "verify by playing", so 1.3 stays unticked until someone plays a battle and the town with the narrator Off.

## Tasks 3.1 and 4.x (user: "go ahead with task 3.1 ... then continue with the consent and Settings screens")

- **3.1:** `heroes_net` links libcurl (`find_package(CURL REQUIRED)`), and `curlTransport` follows §5. CI: `libcurl4-openssl-dev` on both Ubuntu jobs, `vcpkg install curl:x64-windows` with the vcpkg toolchain on Windows, and a key-leak check (`strings` on `heroes` and `heroes-window`) on Linux. The relay job waits for chunk 6.
- The manual network check was a throwaway program in the scratchpad, not a `--net-check` flag, so there was nothing to remove. It POSTed `{}` to https://example.com over TLS and got **405** (example.com refuses POSTs, so 200 isn't possible with a POST), proving the TLS path. A closed local port came back Unreachable.
- **4.1/4.2:** consent screen, Settings screen and `makeNarrator`, as described in `docs/simpledocs/narration.md`. SPEC DRIFT: Settings appears only when the game has a settings file (the real game always does), so screen tests without one keep the old Hub rows. Hosted stays disabled ("Not available in this build") until the relay exists.
- Live replies now replace built-in lines in battles and town (the §6 UI contract). Talk (5.1) is next.

## Task 5.1: Talk

- Built per §7 on all four NPC screens. SPEC DRIFT: Talk is the **T** key (shown as "T talk") on the shared NPC frame rather than a row in each NPC's menu, because the merchant and bank screens are two lists with no menu to add it to. It's available whenever the game has narrator data; with the narrator Off the NPC answers with its built-in line, as §7 says.
- Screens now poll the narrator once a step and hand each reply to whoever asked (greeting or Talk), so one can't swallow the other's reply.
- Verification: clang debug, asan (UBSan clean; Talk and narrator tests repeated 5 times) and ci 671/671; window-off 636/636; mingw syntax check clean. Docker wasn't running, so Ubuntu GCC 13 is left to CI.
