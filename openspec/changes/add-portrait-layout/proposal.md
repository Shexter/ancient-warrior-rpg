# Proposal

## Why

Phone terminals (Termux, iSH, Arch on a phone) held upright are about 40–60 columns wide and 30–50 rows tall. The game needs at least 80×24, so on a phone it only shows the resize prompt. The user wants the game playable there, with the picture on top and the menus underneath, the way phone games are laid out.

## What Changes

- **Portrait layout, styled like a phone arcade game** (user's reference, 2026-10-12: a Pac-Man app with a score header, the board in the middle and a big boxed d-pad below). When the terminal is narrower than 80 columns but at least 48×44, every screen draws a 48×44 portrait layout in three bands:
  - **header:** the screen title with ✕ (Back) on the right, then big-number stats (Lv, HP, Gold; in battle, both HP bars);
  - **scene and menu:** sprites, bars and the battle log, then the screen's menu as full-width boxed rows;
  - **control pad:** large boxed buttons ▲ ◀ ● ▶ ▼ at the bottom (● confirms), each at least 3 rows tall so a thumb hits it.
- **New minimum.** The smallest playable terminal becomes "80×24, or 48×44 in portrait". The resize prompt names both and wraps to fit narrow screens.
- **`--layout portrait`** forces it, for trying it on a desktop; `auto` picks it by size.
- **Taps.** Terminals that report touches as mouse clicks (Termux does) can pick a menu row by tapping it. Keys still work everywhere.
- **Unchanged:** the 80×24 and 120×40 layouts, game logic, saves and content.

## Capabilities

### New Capabilities
- (none)

### Modified Capabilities
- `terminal-rendering`: the minimum terminal size gains the portrait option; adds the portrait layout requirement and tap selection.

## Impact

- **Engine:** the loop's minimum-size rule; input decoding of mouse/touch clicks (SGR mode); terminal setup enables mouse reporting and restores it on exit.
- **App:** a portrait draw path for every screen, built on shared portrait widgets (the header, boxed button menu, control pad, dialogs at most 44 wide). Two-list screens (Almonry, Thesaurus) show one list at a time with a toggle.
- **Tests:** a portrait frame test for every screen at 48×44, the tap tests, and the resize prompt.
- **Order:** after `fun-pass`, whose screens it must lay out.
