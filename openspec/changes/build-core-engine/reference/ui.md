# UI Reference

All screens are designed for exactly **80×24**. In a larger terminal, center the 80×24 canvas and fill the rest with the background color. Coordinates are `(col, row)`, 0-based.

## Palette

| token | RGB | use |
|---|---|---|
| bg | 16,16,24 | background |
| panel | 30,30,46 | box interiors |
| border | 88,91,112 | box lines |
| text | 205,214,244 | normal text |
| dim | 108,112,134 | disabled options, hints |
| accent | 249,226,175 | selected option, titles |
| hp_hi / hp_mid / hp_lo | 166,227,161 / 249,226,175 / 243,139,168 | HP bar above 50% / above 20% / at most 20% |
| mp | 137,180,250 | MP bar |
| dmg | 243,139,168 | damage numbers |
| heal | 166,227,161 | heal numbers |
| gold | 250,179,135 | gold amounts |

## Shared widgets (`src/app/widgets.hpp`)

- **Menu**: a vertical list. The selected row is drawn `▶ label` in accent; other rows `  label` in text; disabled rows in dim, which can't be selected.
  - ↑/↓ (and `w`/`s`) move the selection and wrap, skipping disabled rows.
  - Enter selects.
  - Keys `1`–`9` jump to that option and select it immediately (if enabled).
  - Esc means Back when the screen allows it.
- **TextInput**: a single line of printable ASCII with a max length. Backspace deletes. Enter submits. Esc cancels. A `_` cursor blinks every 500 ms.
- **NumberInput**: digits only, with min/max. A value out of range shows "Enter a number from MIN to MAX" in dmg color and doesn't submit.
- **MessageBox**: a centered box of at most 60 columns with word-wrapped text and "[Enter]" in dim at the bottom right. Enter or Esc dismisses it.
- **Typewriter**: reveals 40 characters per second. Enter reveals the rest of the current block instantly, and a second Enter advances.
- **ConfirmBox**: a MessageBox with `[Y]es / [N]o`. Esc means No.
- **Status panel** (rows 0–3, used on most town screens):
  ```
  ┌──────────────────────────────────────────────────────────────────────────────┐
  │ ARIA  Fighter Lv 14   HP ████████████░░░░░░░░ 250/350   MP ██████░░ 30/50     │
  │ ATK 100  DEF 25  LIFE 0   EXP 1200  Next 500   Gold 20000  W Iron Sword+1 A —  │
  └──────────────────────────────────────────────────────────────────────────────┘
  ```
  The HP bar is 20 cells and the MP bar is 8 (`█` filled, `░` empty). ATK, DEF and LIFE include equipment. "+N" after a weapon or armor name is shown only when its upgrade level is above 1. "—" means the slot is empty.

## Screen flow

```
Title ─ New Game ─▶ Intro (typewriter) ─▶ Create Hero ─▶ Hub
      ─ Continue ─▶ (load save; on error MessageBox + stay) ─▶ Hub
      ─ Quit
Hub ─ Training ─▶ Training Setup ─▶ Training Playback ─▶ Hub
    ─ Mission  ─▶ Mission Select ─▶ Mission Intro ─▶ Battle ─▶ Result ─▶ Hub
    ─ Town     ─▶ Town Menu ─▶ Merchant | Blacksmith | Guildmaster | Bank ─▶ Town Menu
    ─ Save     ─▶ MessageBox "Game saved." (or the error) ─▶ Hub
    ─ Quit     ─▶ ConfirmBox "Quit without saving?" (skipped if saved since last change)
```
Esc on any sub-screen returns to its parent. Esc on the Hub opens the Quit confirm.

## Title

Rows 4–9: the game title "Ancient Warrior RPG" in a 6-row box-drawn banner, centered in accent. Rows 14–16: Menu with New Game, Continue (disabled if no save file exists), Quit. Row 22: dim `↑↓ select · Enter confirm`.

## Create Hero

1. "Enter Hero name (max 8 characters):" with a TextInput (max 8). Invalid input shows the `describe(InvalidName)` message.
2. Show the path-intro story (Typewriter), then a "WARRIOR PATHS" Menu: Fighter, Hunter, Rogue. To the right of the menu (cols 30–78), a preview box shows the highlighted path's stats and flavor text, and the path's hero sprite at col 60, row 8.
3. Enter creates the game and goes to the Hub.

## Hub

The status panel is on rows 0–3. The hero sprite is at col 4, row 6 (24×24 px, so 12 rows). A Menu at col 34, row 7 has Training, Mission, Town, Save, Quit.
- Training and Mission are disabled with a reason shown in dim beside them when `!canAdventure`: "Equip a weapon and armor" or "Fallen: visit the Guildmaster".
- Mission is disabled below level 20 ("Requires Lv 20").
- Row 22 shows the last message (for example "Game saved.").

## Training Setup / Playback

- Setup: NumberInput "Cave level (1–30)", then "Rounds (1–100)".
- Playback, per round:
  - The monster sprite (`monster.sprite`, 16×16 px) sits at col 56, row 6, with its name and HP bar below.
  - The hero sprite is on the left. Both HP bars animate along with the round's events at 10 events per second.
  - The round summary line is "Round 3/10 · Bat defeated · +1000 gold · +1000 exp · Drop: Topaz", or "· Bag full, Topaz lost".
  - Enter skips to the end of the current round. Esc skips all remaining playback straight to the totals screen (the results are already applied).
- Totals: rounds won, gold, exp, levels gained, drops. Defeat shows "ARIA has been injured badly" in dmg color.

## Mission Select / Intro

A Menu lists every mission from data plus 2 disabled "Coming Soon" rows. A completed mission is disabled with "Cleared". The intro shows the story lines with the Typewriter, then "Enter the lair? [Y]es / [N]o".

## Battle (80×24)

```
row 0   MEDUSA LAIR                                                     Turn 3
rows 2–15  arena: hero sprite (24×24 px → 12 rows) at col 6,row 4 · enemy sprite at col 44,row 2
row 16  ARIA  HP ████████████░░░░░░░░ 250/350      MEDUSA HP ████████████████████ 3000/3000
row 17        MP ██████░░ 30/50  [REFLECT 2]              [STUN 3] [DEF↓ 3]
rows 18–21 battle log: the newest 4 lines, no border (oldest at top), text color; row 17 above it is the status row
row 22  ▶ Attack    Skill    Flee                                        (actions)
row 23  dim hint line: ←→ select · Enter confirm · during animation: Enter skips
```
- The actions row is a horizontal Menu: ←/→ (and `a`/`d`) move, and `1`/`2`/`3` are shortcuts.
- Skill opens a boxed vertical Menu overlay at rows 14–20, cols 16–79 (border + 5 skill rows + border), drawn over the arena and log. Each row shows `Name  (MP)  description`, with unaffordable or unlearned skills in dim. Esc returns to the actions row.
- If an action is rejected (`ActionError`), show its message in the log and keep the turn.
- `[DEF↓ 3]` uses the glyph `v` instead of `↓` to keep everything single-width: `[DEF v 3]`.

### Event animations

Events from `act()` play strictly in order. Input other than Enter (skip) is ignored until the queue is empty.

| event | animation | duration | log line |
|---|---|---|---|
| TurnStarted Hero | none, then actions row enabled | 0 | — |
| TurnStarted Enemy | none | 0 | — |
| Attacked | attacker sprite moves 3 cols toward the target and back | 300 ms | "ARIA attacks!" / "MEDUSA attacks!" |
| SkillCast | skill name in a centered accent banner on row 9, plus screen shake (whole frame offset ±1 col, alternating every 50 ms) | 800 ms | "ARIA casts FROZEN Throne!" |
| Damaged (amount > 0) | target flashes (Flash tint, 2× 75 ms on/off) and a floating "-N" in dmg color rises 3 rows above the target | 300 ms (number keeps floating 700 ms in parallel) | "MEDUSA takes 380 damage." Reflect cause: "380 damage reflected to MEDUSA." |
| Damaged (amount 0) | floating "BLOCK" in dim | 300 ms | "MEDUSA's armor absorbs the blow." |
| Healed | floating "+N" in heal color | 300 ms | "ARIA recovers 25 HP." |
| HpHalved | target flashes, row 9 banner "HP HALVED" | 500 ms | "MEDUSA's life force is torn in half!" |
| StatusApplied | the status tag appears under the HP bar | 200 ms | "MEDUSA is frozen for 3 turns." / "Defense lowered for 3 turns." / "ARIA is shielded by a Golden Bell." |
| StatusExpired | the tag disappears | 200 ms | "MEDUSA thaws." / "MEDUSA's defense recovers." / "The Golden Bell fades." |
| TurnSkipped | target sprite drawn with Dim(3) | 500 ms | "MEDUSA is frozen and cannot move!" |
| ExtraTurn | row 9 banner "AGAIN!" | 500 ms | "MEDUSA strikes again!" |
| Defeated | sprite fades Dim(0→8) over the duration | 800 ms | "MEDUSA has been defeated!" / "ARIA has fallen..." |
| Fled | hero sprite slides off-screen to the left | 500 ms | "ARIA flees the battle." |
| BattleEnded | goes to the Result screen | — | — |

**Result**: Victory lists exp, gold and items (reporting any item lost to a full bag). Defeat shows "All equipment destroyed!" in dmg color. Fled and Draw show "You escaped." / "The battle was a stalemate."

## Town screens

Every NPC screen shows the status panel on rows 0–3, the NPC title on row 5 in accent, and its content on rows 6–21. Results go in a MessageBox using `describe(TownError)` text on failure.

- **Merchant**: two side-by-side lists. "For sale" (cols 1–38) shows price > 0 items with their prices. "Your bag" (cols 41–78) shows the 10 slots, with "(empty)" in dim. Tab switches focus. Enter on a for-sale item asks "How many? (1–99)", then buys. Enter on a bag item opens a Menu: Sell (if price > 0, asks the quantity), Auction (if auctionValue > 0, plays the bids one per 600 ms: "Bid accepted at 550…", then "Merchant fee is 10%. You receive 495 gold."), Cancel. A dead hero sees only the MessageBox "I have no business with you!" and returns.
- **Blacksmith** (dead hero: "Empty soul, you don't belong here"): Menu with Change Equipment, Upgrade, Forge, Back.
  - Change: pick a slot, then a list of matching bag items. If the current upgrade level is above 1, a ConfirmBox warns "Your upgraded weapon will return to Lv 1. Continue?".
  - Upgrade: pick a slot. The screen shows the cost (material ×L, exp, gold) with each line green if affordable and red if not. Then a ConfirmBox.
  - Forge: list the recipes with ingredients (have/need counts) and the success chance `forgeLevel × 10%`. Then a ConfirmBox, then "Forging SUCCESS!!! You now possess Fire Claymore" or "Forging FAILED, the materials are lost...".
- **Guildmaster**: on entry with HP 0, revive automatically and show "Great warrior, be careful next time..." with the gold cost. Then a Menu with Restoration, Build Charisma, Learn Skill, Change Name, Back.
  - Restoration: list bag consumables, Enter uses one.
  - Charisma: a stat Menu, then a NumberInput "EXP to sacrifice" with a live preview "→ +G MaxHp for P×100 gold", then a ConfirmBox.
  - Learn Skill: the 5 skills with requirements, each green or red.
  - Change Name: "The fee is 50000 gold", then a TextInput.
- **Bank**: without an account, a ConfirmBox "Open an account for 10000 gold?". With an account, side-by-side lists Bank | Bag. Enter on an item asks the quantity and moves it.

## Hero sprites and placeholders

Chunk 9 ships hand-written placeholder sprites. Real art replaces them later through `sprite_bake` without code changes.

| file | size (px) | content |
|---|---|---|
| `hero_fighter.sprite` | 24×24 | stocky figure, red tunic |
| `hero_hunter.sprite` | 24×24 | figure with green cloak, bow |
| `hero_rogue.sprite` | 24×24 | slim figure, purple hood |
| `medusa.sprite` | 32×28 | serpent-haired figure, green/teal |
| `monster.sprite` | 16×16 | generic beast silhouette, brown |
