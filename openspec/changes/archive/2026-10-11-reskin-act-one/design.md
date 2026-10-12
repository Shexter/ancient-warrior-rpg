# Design

## Context

See proposal.md for why. Everything the player reads about content (names, descriptions, flavor, title, story) already comes from `data/*.json`, which the loader validates at startup. Screen chrome (bar labels, screen titles, NPC lines) is string literals in `src/app/`. Saves store IDs only. The Medusa golden vectors check event sequences, which carry `enemyId` and the enemy's name but no item or path names. The Hub unlocks Mission at the lowest `minLevel` in the data.

## Goals / Non-Goals

**Goals:**
- Swap all player-facing text to Act I in one change, with saves and golden vectors untouched.
- Add the Avaritia mission using only existing mechanics, and pin its difficulty with the simulator.

**Non-Goals:**
- No new mechanics: Avaritia's gold theft waits for `add-lua-scripting`.
- No renaming of IDs, stats, save fields, or C++ identifiers (`maxMp` stays `maxMp`).
- No localization layer (Latin/English toggle); world-bible §10.4 keeps that for later.
- No new hero sprites. The existing three still fit Knight, Hunter and Pilgrim.

## Decisions

**1. Rename through data, not code.** Display names live in JSON, so the re-skin is a data edit plus a short list of UI literals. Alternative: a theme file mapping old to new names. Rejected: it adds a lookup layer for text that already has a single home.

**2. "Fervor" is a label, not a rename.** Only the strings the player reads change ("FV" on the 8-cell bars where space is tight, "Fervor" elsewhere). The stat stays `mp`/`maxMp` in code and saves, so no save migration is needed and the existing scenarios that say "MP" still describe the mechanic. Alternative: rename the stat everywhere. Rejected: it breaks saves and churns every file for no player-visible gain.

**3. The existing main-spec scenarios keep their original item names.** Scenarios in `town-services`, `combat` and `hero-progression` say "Topaz", "Iron Sword", "Frozen Throne" and so on. They identify the same IDs, and `reference/act-one-names.md` maps every ID to its new name. Only the two `game-content` requirements whose wording is about names are modified. Alternative: rewrite about 15 requirements just to change nouns. Rejected for now: a large MODIFIED delta that changes no behavior. It can be done later as a docs-only change.

**4. Avaritia is a new enemy with `medusa_pattern`.** Reusing the one behavior the engine has keeps the change data-only and leaves Medusa's definition, and so her golden vectors, byte-identical. Its stats are tuned with `heroes-sim` into the band in the balance-simulator delta. Measured during planning (seed 7, `smart`, Sword +5 / Iron Shield +5, two skills): 2000/150/15/5 gave 87/93/83% (Knight/Hunter/Pilgrim), and 2400/160/15/5 gave 12/48/49%. So the starting point is 2200/150/15/5, adjusted until all three paths sit in 40–80%. The band is checked by a CTest using the simulator library with fewer battles, so a data edit that breaks balance fails CI.

**5. Mission order comes from the data file order.** Mission Select already lists missions in file order, so Avaritia goes first in `missions.json`. No sorting code.

**6. Layouts must fit the longer names.** The longest new names are "Fragment of the Cross" (21) and "Let There Be Light" (18). The fixed columns that assumed short names get adjusted:
- the skill overlay's name column, now padded to 18;
- the Learn Skill requirement columns, which start after the longest skill name.

Every other list already measures its labels.

**7. Avaritia's sprite comes from the existing art pipeline.** It is made like the others: Draw Things (FLUX.2 [klein] 4B), chroma key, fitted to 32×28, quantized, then `sprite_bake`. The prompt and seed are recorded in `tools/art/README.md`.

## Risks / Trade-offs

- [Balance band is narrow because outcomes cliff with gear] → The CTest pins the shipped numbers, and the band is wide (40–80%). If no stat set fits all three paths, report it rather than widen the band silently.
- [Sacred names in a game (Fragment of the Cross, St. Michael)] → The user chose these (world-bible §9) and reviews the text. Descriptions stay short and reverent (world-bible §7).
- [Screen tests assert old strings] → Expected churn: these are display assertions, updated with the text. Golden, content-rule, save and battle tests must pass unchanged, and any change to them is a red flag.
- [Spec scenarios keep old nouns (Decision 3)] → The mapping table makes them traceable. Listed as a follow-up in the final report.

## Migration Plan

None needed: saves are ID-based. An old save loads and shows the new names (a game-content scenario). Rollback is reverting the data and UI-text commits.
