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
- **2.x** (the async narrator with a fake transport) is local and could go next.

Verification: clang debug, asan (UBSan clean), ci and GCC 14 Release `-Werror` 639/639; window-off 604/604; mingw syntax check clean.
