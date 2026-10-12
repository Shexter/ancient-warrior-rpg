# Proposal

## Why

Phone terminals (Termux, iSH, Arch on a phone) held upright are about 40–60 columns wide and 30–50 rows tall. The game needs at least 80×24, so on a phone it only shows the resize prompt. The user wants the game playable there, with the picture on top and the menus underneath, the way phone games are laid out.

## What Changes

- **Portrait layout.** When the terminal is narrower than 80 columns but at least 40×32, every screen draws a 40×32 portrait layout:
  - the top half is the scene: the status summary, sprites, HP bars and the battle log;
  - the bottom half is the menu: full-width rows numbered 1–9 for one-thumb play, with `0 Back` always last.
- **New minimum.** The smallest playable terminal becomes "80×24, or 40×32 in portrait". The resize prompt names both.
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
- **App:** a portrait draw path for every screen, built on shared portrait widgets (a 3-row status strip, a full-width bottom menu, dialogs at most 36 wide). Two-list screens (Almonry, Thesaurus) show one list at a time with a toggle.
- **Tests:** a portrait frame test for every screen at 40×32, the tap tests, and the resize prompt.
- **Order:** after `fun-pass`, whose screens it must lay out.
