# Design

## Context

See proposal.md. The per-frame canvas choice from `add-large-art-layout` already lets each screen pick its canvas from the terminal size; portrait is a third size. Screens draw by the frame they are given (`isLarge(frame)`), so a portrait path is one more branch per screen.

## Decisions

**1. A fixed 40×32 portrait canvas, centered.** One size to design and test against, like 80×24 and 120×40. Wider or taller phones get margins. 40 columns fits a 32-wide compact sprite plus borders. 32 rows leaves about 16 for the scene and 14 for the menu. The constant is chosen from typical phone terminals and will be checked against the user's own `tput cols; tput lines` before chunk 2.

**2. Shared portrait widgets do most of the work.**
- `drawStatusStrip` is 3 rows: name, level, gold; then the HP and Fervor bars; then ATK/DEF and the gear, abbreviated.
- `drawBottomMenu` puts full-width numbered rows from row 18, with notes on the line under the selected row.
- Dialogs are at most 36 columns wide.
- Screens then mostly place their scene in rows 0–16 and hand their existing `Menu` to the bottom menu.
- Alternative: hand-lay every screen. Rejected, because the split is the same everywhere.

**3. Screen-specific choices:**
- **Battle:** the enemy sprite (compact) on top and the hero shown only as name and bars, because two sprites don't fit 40 columns. The newest 3 log lines, then the five actions as rows; the Skill and Item overlays replace the bottom menu.
- **Almonry and Thesaurus:** one list at a time ("For sale" or "Your bag"), switched with Tab or a `Switch list` row.
- **Create Hero:** the path preview above, without the sprite if the flavor text needs the room.

**4. Taps come in as mouse clicks.**
- The terminal turns on SGR mouse reporting (`?1000h` and `?1006h`), and turns it off on every exit path, alongside the existing restore.
- The input decoder parses `ESC [ < b ; x ; y M` presses into a new `Key::Click` with a cell position.
- `Menu` maps a click on one of its drawn rows to choosing that row. Clicks elsewhere are ignored.
- Alternative: rely on number keys only. That stays as the fallback, but taps are what phone players expect.

**5. `--layout portrait`** forces it at any size, so it can be tested on a desktop.

**6. Taps are the primary input on phones (user's Termux screenshots, 2026-10-12).** With the keyboard hidden the terminal is about 48×48; with it open, only about 48×15, too small for any layout. So phone play happens with the keyboard hidden, and every screen must be fully playable by tapping alone, including Back and confirm/cancel in dialogs (tappable `[Yes]` / `[No]` / `[OK]`). Tap support is therefore chunk 1, not an extra.

## Risks / Trade-offs

- [Phone keyboards lack arrows and Esc] → Every menu is reachable by number, and `0` is Back in portrait. Termux's extra-keys row also provides arrows and Esc.
- [Mouse reporting left on after a crash garbles the shell] → It's turned off on the same restore paths as raw mode and the alternate screen, including signals; tests cover the restore bytes.
- [Some phone terminals ignore mouse reporting] → Keys still work, and tapping is purely additive.
- [40×32 may not match the user's phone] → It's a single constant, confirmed before chunk 2.
