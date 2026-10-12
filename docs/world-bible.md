# World Bible — DRAFT

Status: **Act I approved for the re-skin (2026-10-12)**, with the decisions in §9. Acts II and III remain drafts. Nothing here is implemented until its OpenSpec change (`reskin-act-one`) is applied.

Every Latin term is a first-pass suggestion and needs a Latin check before it ships. Items marked **[open]** need the author's decision.

---

## 1. Premise

*Civitas Lucis*, the City of Light, is a cathedral-city that has stood for a thousand years. Something is breaking in over its walls: the sins of the world, grown strong enough to take shape as demons. The Order of Light sends out knights to fight them.

You play one of the Order's newest members. The fighting is real, but the story is about what the fighting costs and what it is for. Strength comes from discipline, from grace, and from the people around you. It does not come from levels.

This matches a rule already in the engine: **leveling does not raise stats**. Power comes from equipment, upgrades, and Charisma. In this world that reads as *"you do not grow by experience alone, but by what you are given and what you choose to carry."*

## 2. Three acts, three settings

The game's three mission slots become three acts. The core engine ships Act I only. Acts II and III are later data-only additions (new missions, enemies, story text, and Lua mods).

| Act | Setting | Feel | Shipped by |
|---|---|---|---|
| I | **Civitas Lucis**: the cathedral-city under siege | Busy, communal, urgent. This is the Hub. | the core re-skin |
| II | **Via Peregrinorum**: the pilgrim road | Episodic. One stop, one temptation, one boss. | later change |
| III | **Monasterium Ultimum**: the monastery at the edge of the world | Quiet, austere, reflective. The final area. | later change |

The arc is **defense, then journey, then arrival**: first you defend your home, then you leave it and are tested, and finally you reach a place of silence and face the root of everything.

### Act I: Civitas Lucis (the Hub)

| Engine feature | In the world |
|---|---|
| Hub | The cathedral square |
| Training (caves 1–30) | The catacombs and the surrounding countryside, deeper levels being older and darker |
| Merchant | The Almonry. It sells simple goods and buys relics for the poor. |
| Blacksmith | The Forge of the Order, run with a chaplain who blesses each piece |
| Guildmaster | The Prior of the Order. He gives skills and revives the fallen. |
| Bank | The Church's treasury (*Thesaurus*). Gold deposited here is safe. |
| Save | Keeping a *Liber Vitae*, the book of the hero's deeds |

### Act II: Via Peregrinorum

A road of seven stops, one per sin. Each stop is a small place with its own demon, and each is a temptation as much as a fight. The fights are short and the story beats between them carry the weight.

### Act III: Monasterium Ultimum

A fortress of silent monks where the road ends. The player arrives changed, and the final boss is the root sin, **Superbia**.

## 3. Hero paths

Same stats as `content-tables.md`. Only names and flavor change. IDs stay `fighter`, `hunter`, `rogue`.

| id | Name | Latin | Flavor |
|---|---|---|---|
| `fighter` | Knight | *Miles* | A knight of the Order, blessed with the Strongest Fist |
| `hunter` | Hunter | *Venator* | A ranger who tracks the demon-touched, a body strong as rock |
| `rogue` | Pilgrim | *Peregrinus* | A wandering scholar-confessor, a balanced mind |

## 4. Items

IDs stay as they are. **Only `name` and `description` change.** That keeps code, saves, and golden tests untouched.

| id | New name | Latin | Note |
|---|---|---|---|
| `iron_sword` | Sword | *Gladius* | The starting weapon |
| `fire_claymore` | Flaming Sword | *Gladius Flammeus* | Echoes the flaming sword of the cherub at Eden's gate |
| `thors_hammer` | Mace of St. Michael | *Clava Sancti Michaelis* | The boss reward. Maces were the bearer's weapon of choice for clergy. |
| `steel_shield` | Iron Shield | *Scutum Ferreum* | The starting armor |
| `diamond_plate` | Adamant Cuirass | *Lorica Adamantina* | |
| `peace_maker` | Armor of Light | *Arma Lucis* | After Romans 13:12 |
| `topaz` | Rosary | *Rosarium* | Key for *Angelus Custos*. A prayer made physical. |
| `gold_leaf` | Martyr's Relic | *Reliquia Martyris* | Key for *Ictus Iustus* |
| `frozen_tears` | Tears of Penance | *Lacrimae Paenitentiae* | Key for *Scutum Fidei* |
| `broken_staff` | Broken Crozier | *Baculus Fractus* | Key for *Exorcizo Te* |
| `pure_ice` | Fragment of the Cross | *Fragmentum Crucis* | Key for *Fiat Lux*. **[open]** see §9. |
| `fire_dust` | Altar Ember | *Favilla* | Weapon upgrade material |
| `turtle_shell` | Consecrated Plate | *Lamina Sacra* | Armor upgrade material |
| `gold_dust` | Gold Dust | *Pulvis Aureus* | Gilding from icons. A forge ingredient. |
| `black_pearl` | Dark Pearl | *Margarita Obscura* | A forge ingredient, a temptation of wealth |
| `broken_compass` | Broken Compass | *Acus Fracta* | A pilgrim's lost guide. A forge ingredient. |
| `phoenix_feather` | Phoenix Feather | *Penna Phoenicis* | The phoenix is an old Christian symbol of resurrection |
| `health_potion` | Holy Water | *Aqua Benedicta* | Restores HP |
| `mana_potion` | Holy Oil | *Oleum Sanctum* | Restores MP |
| `eternal_youth` | Viaticum | *Viaticum* | The last rites. Fully restores HP and MP. |

Renaming `mana_potion` makes the "MP" resource read as *spirit* or *fervor*. **[open]** whether to also relabel the MP bar.

## 5. Skills

Same mechanics as `content-tables.md`. Only names and descriptions change.

| id | New name | Latin | Virtue it echoes |
|---|---|---|---|
| `angel_wings` | Guardian Angel | *Angelus Custos* | Hope |
| `meteor_fist` | Righteous Strike | *Ictus Iustus* | Justice |
| `golden_bell` | Shield of Faith | *Scutum Fidei* | Faith |
| `frozen_throne` | Exorcism | *Exorcizo Te* | Fortitude |
| `supernova` | Let There Be Light | *Fiat Lux* | Charity (the finisher) |

## 6. The seven sins and their virtues

Each boss is a sin given form. Each has a **virtue that opposes it**, which becomes the theme of the reward and of the quiet scene after the fight.

| Sin | Virtue | Boss idea (mechanics are proposals) |
|---|---|---|
| *Avaritia* (greed) | *Liberalitas* (generosity) | Steals gold on hit and hoards it. Dropping it returns the gold. |
| *Gula* (gluttony) | *Temperantia* (temperance) | Heals itself by a share of the damage it deals. Short fights favor you. |
| *Ira* (wrath) | *Patientia* (patience) | Hits harder as its HP falls. Patience means waiting out the early turns. |
| *Luxuria* (lust) | *Castitas* (chastity) | Charm-style status that makes the hero skip a turn. Uses the existing `stun` mechanic. |
| *Acedia* (sloth) | *Diligentia* (diligence) | Slows the fight: it skips turns, and the fight turn-cap matters. |
| *Invidia* (envy) | *Caritas* (charity) | Copies the hero's ATK. A mirror fight. |
| *Superbia* (pride) | *Humilitas* (humility) | The final boss. It reflects damage and punishes using the finisher. |

Distribution across acts:

- **Act I (three bosses in or near the city):** Avaritia, Gula, Ira. These are the sins that spread through a city.
- **Act II (three stops on the road):** Luxuria, Acedia, Invidia. These are the temptations of the journey.
- **Act III (one boss):** Superbia, the root of the rest.

**Medusa** stays as the placeholder mission until the core is playable (§10).

## 7. Tone and moral frame

- **Demons are tempters and corrupters, not mere monsters.** Fighting them is defense, and the story should give each a short, human reason.
- **Strength comes from grace, not magic.** Skills are prayers and blessings, never "power from within."
- **Mercy is a theme.** The free revive when you are broke at 0 HP (already in the engine) is framed as the Order's mercy. Nobody is abandoned.
- **Penance, not punishment.** Dying and recovering costs resources but never ends the story.
- **No mockery.** The tone is reverent but not preachy: short, concrete scenes and no sermons.
- **Honest about difficulty.** Doubt and weakness are allowed in the characters. Faith is shown through action.

### LLM narrator guidance (for the later `add-llm-game-master` change)

- Style: a knight's chronicle, two sentences per event, past or present tense consistently.
- Vocabulary: Latin terms for named things, plain English otherwise.
- Never quote scripture at length, and never invent doctrine. The narrator describes events and does not teach.
- The output is untrusted text as the engineering contract requires, so tone guardrails go in the system prompt, and nothing in the UI relies on them.

## 8. Story text (draft)

Replaces the 7-line opening and 4-line path intro in `story.json`.

**Opening (7 lines):**

1. "For a thousand years the bells of Civitas Lucis have rung at dawn."
2. "The Order of Light kept the gates, and the gates held."
3. "Then the bells stopped answering."
4. "The sins of the world took shape, and they came for the city."
5. "Most knights were sent to the walls. A few were sent to the road."
6. "You are the newest of the Order, and the least proven."
7. "Light does not ask who is ready. It asks who will go."

**Path intro (4 lines):**

1. "The Order calls three kinds of souls to its work..."
2. "the Miles, who stands where others fall"
3. "the Venator, who hunts what hides"
4. "and the Peregrinus, who walks where there is no road."

**Title:** *Miles Christi*, "Soldier of Christ", or *Ordo Lucis*, "Order of Light". **[open]** The first is more direct and the second is gentler.

## 9. Decisions and open questions

Decided by the user, 2026-10-12:
1. **Title:** *Ordo Lucis* ("Order of Light").
2. **Fragment of the Cross:** keep it as the *Fiat Lux* key item, as drafted.
3. **Theological accuracy bar:** the user reviews the text.
4. **MP label:** relabel MP as **Fervor** everywhere the player sees it.
5. **Medusa:** kept unchanged as a bonus mission (so her golden vectors stay valid). Act I gets a new mission against *Avaritia*, which uses Medusa's existing fight pattern until the Lua change gives it gold theft.
6. **Real saints:** named saints are allowed where drafted (Mace of St. Michael).

Still open:
7. **Act II format:** seven short fights, or fewer stops?

## 10. How this lands in the project

1. **Finish the core engine first** (build-core-engine chunks 4–11). No mechanic changes.
2. **Approve this bible**, then run an OpenSpec change, for example `reskin-act-one`, that:
   - changes only `name`, `description`, `flavor`, and story text in `data/*.json`;
   - keeps all IDs, so saves, code, and golden vectors are unaffected;
   - adds the first sin boss as a **new** enemy. It does not replace Medusa, so the Medusa golden vectors stay valid.
3. **Acts II and III** are separate later changes, using the Lua scripting change for boss behaviors.
4. **A localization question:** names and story text are currently in JSON, so a Latin/English toggle is a possible later change.

Anything that needs new mechanics, such as Avaritia's gold theft or Invidia's stat copy, goes through the Lua scripting change and a new OpenSpec spec. It is not part of the re-skin.

## 11. Art direction: ASCII world view (idea, not approved)

Inspired by a reference video, "Water reflection test" from an ASCII MMORPG. Nothing here is in OpenSpec. It would be its own change after the core is done, for example `add-ascii-world-view`.

**Look**
- A top-down 2D view, with the hero sprite near the center and a small name label above it.
- **Luminance ramp:** each cell's glyph is chosen by brightness. Dark areas use sparse marks (`. , ~ -`), mid tones use `( ) / \`, and bright areas use dense glyphs (`Q 8 0 @ $ &`).
- **Per-cell colour:** the foreground is sampled from the source art over a near-black background.
- **Texture glyphs:** grass is many small `|` and `'` blades with scattered `o * .` flowers. Water is `~` and `≈`. Ripples are rings of parentheses.
- **Water reflection:** the cells above a waterline are copied, flipped, dimmed, and shifted sideways by a slow sine wave. The hero and the moon reflect in it.
- Dark, low-key palette with a moon and glints on the water, which suits Civitas Lucis at night.

**How it fits the engine**
- It uses the existing `Cell{glyph, fg, bg}` framebuffer and diff renderer. It needs no engine change.
- The ramp conversion belongs in the bake tool (`sprite_bake`) or a small helper, and the reflection is a framebuffer pass using the existing dim tint.
- Only single-width glyphs are used (ASCII and block characters), as the engine requires.
- At 80x24 there are far fewer cells than in the video, so it will read as chunky ASCII art, not fine detail.

**Candidate scope for the change**
1. A glyph ramp and its bake option.
2. A reflection pass over a waterline.
3. A walkable Act I square (the cathedral square) with the catacomb entrance, and name labels.
4. Movement and collision in a tile map. This is new mechanics, so it needs its own spec.


**Art test, 2026-10-11 (decided: do this after core)**
- A Draw Things → ASCII test on the Fighter (see `tools/art/README.md`) produced four looks. The user liked **48x48 half-block sprites** and the **colored glyph ramp** (brightness picks the character, the image's own colors tint it) best.
- Neither fits the 80x24 canvas: 48x48 half-blocks are 24 rows tall, and the ramp figure was 60x37. So core ships 24x24 sprites from the same pipeline, and the post-core change adds:
  5. A larger layout (for example 120x40) when the terminal is big enough, with 80x24 kept as the fallback.
  6. A glyph-ramp sprite mode (character + color per cell) alongside the half-block `.sprite` format.
