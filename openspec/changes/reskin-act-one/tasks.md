# Tasks

## 1. Act I names in data

- [x] 1.1 Update `data/paths.json`, `items.json`, `skills.json` and `story.json` (title, opening, pathIntro) to `reference/act-one-names.md`, changing only `name`, `description`, `flavor`, `title` and story lines; verify `git diff data/` shows no ID, number or key changes (a script compares every non-text field before and after)
- [x] 1.2 Rename the Medusa mission to `Bonus: "The Gorgon"` in `missions.json` with nothing else changed; verify the Medusa golden-vector tests pass with `tests/battle_golden_data.hpp` unchanged
- [x] 1.3 Widen the skill overlay name column and move the Learn Skill requirement columns past the longest skill name (design Decision 6); verify with screen tests that "Let There Be Light" and "Fragment of the Cross" rows don't overlap their neighbours
- [x] 1.4 Update screen tests that assert the old display text; add a test that a save written with the old data (`health_potion` in the bag, `iron_sword` equipped) loads and shows "Holy Water" and "Sword"; verify all presets, GCC and the MinGW syntax check pass
- [x] 1.5 Update SimpleDocs (`content.md`, `app.md`) for the new names; verify no SimpleDocs page still describes the old names as current
- [x] 1.6 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 2. Player-facing vocabulary

- [ ] 2.1 Relabel MP as Fervor in the UI per the vocabulary table (status panel and battle "FV", Create Hero preview, skill overlay header, "Not enough Fervor.", Charisma "Max Fervor", Restoration message); verify a screen test finds no visible "MP" label on the Hub, Battle, Create Hero and Guildmaster screens
- [ ] 2.2 Rename the Town menu, NPC screen titles, Training titles and the Hub's fallen reason, and re-voice the four NPC lines per the table; verify the existing town, training and hub screen tests updated to the new strings pass
- [ ] 2.3 Update SimpleDocs `app.md` (screen text) and record the vocabulary in `docs/world-bible.md` as implemented; verify the docs match the screens
- [ ] 2.4 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 3. The Avaritia mission

- [ ] 3.1 Generate `assets/sprites/avaritia.sprite` (32×28) with the Draw Things pipeline and record prompt and seed in `tools/art/README.md`; verify it loads through `loadSprites` and shows the user a preview before committing
- [ ] 3.2 Add the `avaritia` enemy and mission (first in `missions.json`) per the reference table, starting at 2200/150/15/5; verify content loads and Mission Select lists Avaritia, then the Medusa bonus, then two "Coming Soon" rows
- [ ] 3.3 Tune Avaritia's stats with `heroes-sim` (seed 7, 20,000 battles, `smart`) until every path is 40–80% with Sword +5 / Iron Shield +5 and the two skills, and below 5% with starting gear; record the runs in `docs/benchmarks.md`. If no stats fit all three paths, stop and report
- [ ] 3.4 Add a CTest that runs the band check through the simulator library with fewer battles (fast enough for CI) and fails if the shipped stats leave the band; verify it passes, and fails when Avaritia's ATK is edited up by 50 in a temporary data copy
- [ ] 3.5 Add screen tests: the Hub unlocks Mission at level 15 ("Requires Lv 15" below); a battle against Avaritia reaches a Result screen and its victory rewards are Gold Dust and Dark Pearl; verify all presets, GCC and MinGW pass
- [ ] 3.6 Play it once on the real binary (save edited to level 15 with Sword +5 / Iron Shield +5) and confirm the intro, battle and result read correctly; update SimpleDocs and mark world-bible Act I as shipped
- [ ] 3.7 Complete chunk quality check against `docs/engineering/code-correctness.md`.
