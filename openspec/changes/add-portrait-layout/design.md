# Design

## Context

See proposal.md. The per-frame canvas choice from `add-large-art-layout` already lets each screen pick its canvas from the terminal size; portrait is a third size. Screens draw by the frame they are given (`isLarge(frame)`), so a portrait path is one more branch per screen.

## Decisions

**1. A fixed 48×44 portrait canvas, centered.** One size to design and test against, like 80×24 and 120×40. Wider or taller phones get margins. The user's screenshot of the resize prompt clipped at exactly 48 columns, with about 47 rows with the keyboard hidden. Rows: header 0–5, scene 6–21, menu 22–31, control pad 32–43. Still confirmed with `tput` in task 3.1.

**1a. Arcade look (user's Pac-Man reference).** The existing dark background and thin light-blue box lines. The header uses a dim label over a large value ("LEVEL" over "7"), like SCORE / HIGH. Menu rows and pad buttons are boxes with the label centered, so they read as buttons. The pad is a plus shape: ▲ on top, ◀ ● ▶ in the middle (● = Enter), ▼ below, each box 14×3. The ✕ in the header is Back (Esc), and so is a boxed "✕ Back" beside ▲ (added after phone testing: the corner ✕ was too easy to miss, and a player had to open the keyboard for Esc). The pad sends the same keys as the keyboard, so screens need no pad-specific logic.

**2. Shared portrait widgets do most of the work.**
- `drawHeader`: title and ✕, then label-over-value stats.
- `drawButtonMenu` puts full-width boxed rows (numbered, tappable) in rows 22–31, scrolling when a menu is longer.
- `ControlPad` draws the five buttons and turns a tap on one into Up/Left/Enter/Right/Down.
- Dialogs are at most 44 columns wide, with `[Yes]` `[No]` `[OK]` as boxes.
- Screens then place their scene in rows 6–21 and hand their existing `Menu` to the button menu.
- Alternative: hand-lay every screen. Rejected, because the split is the same everywhere.

**3. Screen-specific choices:**
- **Battle:** the enemy sprite (compact) on top and the hero shown only as name and bars, because two sprites don't fit 48 columns. The newest 3 log lines, then the five actions as rows; the Skill and Item overlays replace the bottom menu.
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
- [48×44 may not match the user's phone] → It's a single constant, confirmed before chunk 2.
- [The pad takes 12 rows] → It makes tap-only play comfortable; menu rows are tappable too, so the pad is mainly for moving through lists and battle.
