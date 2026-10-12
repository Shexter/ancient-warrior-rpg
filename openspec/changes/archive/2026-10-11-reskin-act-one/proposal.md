# Proposal

## Why

The core engine still wears the 2021–22 placeholder theme ("Ancient Warrior RPG", Medusa, Thor's Hammer). The world bible's Act I, *Civitas Lucis*, is now approved with the user's decisions (world-bible §9, 2026-10-12), and the core is finished, so the theme can land without touching any mechanics.

## What Changes

- The game is titled **Ordo Lucis**. Paths, items, skills and story text take their Act I names and descriptions (Knight / Hunter / Pilgrim, Sword, Holy Water, Guardian Angel, Fragment of the Cross, ...). The full table is in `reference/act-one-names.md`.
- Every **ID stays the same**, so saves, golden vectors and the simulator keep working. Only `name`, `description`, `flavor`, `title` and story lines change in `data/*.json`.
- The MP resource is shown to the player as **Fervor** (status panel, battle, skill costs, potions, Charisma, path preview). The stat and its save field are unchanged.
- The town speaks in Act I terms: the Almonry, the Forge of the Order, the Prior and the Thesaurus, with re-voiced NPC lines.
- A new Act I mission, **Avaritia** (greed), is added as a new enemy and mission. It fights with Medusa's existing pattern until the Lua change gives it gold theft, and is tuned with the simulator into a 40–80% win band for a mid-game build.
- **Medusa is kept, unchanged as an enemy**, and her mission becomes a bonus mission listed after Avaritia. Her golden vectors stay valid.
- Mission requirements shift: the Hub's Mission option now unlocks at Avaritia's level (15) instead of 20.

## Capabilities

### New Capabilities
- (none)

### Modified Capabilities
- `game-content`: the shipped names, descriptions and story are the Act I set; the Stable-ID scenario names the re-skinned item; adds the player-facing vocabulary (title, Fervor, town names) and the Act I mission list.
- `balance-simulator`: adds the balance band the Avaritia mission must meet.

## Impact

- **Data:** `data/items.json`, `skills.json`, `paths.json`, `story.json`, `missions.json`, `enemies.json`.
- **Art:** a new `assets/sprites/avaritia.sprite` (32×28, from the Draw Things pipeline in `tools/art/`).
- **Code (UI text only):** `src/app/widgets.cpp` (status panel), `battle_view.cpp`, `screens/{new_game,hub,town,merchant,blacksmith,guildmaster,mission}.cpp`. No game-logic changes.
- **Tests:** screen tests that assert display text are updated; golden vectors, content, save and battle tests are untouched.
- **Docs:** SimpleDocs (`app.md`, `content.md`), `docs/world-bible.md` status, `tools/art/README.md`.
- **Saves:** fully compatible. An existing save loads and shows the new names.
