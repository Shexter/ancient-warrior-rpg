# App: screens and widgets

**Command line:** `heroes [--seed N] [--data-dir PATH]`. Unknown options or bad values print the error and usage to stderr and exit 2. The data directory is `--data-dir`, else `HEROES_DATA_DIR`, else the source tree's `data/`. Content errors are printed one per line and the game exits 2 **before** the terminal is switched to raw mode or the alternate screen (a CTest case checks this). If no save location can be found (no `HOME`/`APPDATA` and no `HEROES_SAVE_DIR`) it exits 2. Other startup failures (for example not running in a terminal) print `heroes: error: ...` and exit 1 after the terminal is restored.

**Session:** content, the current `GameState` (none until a game is started or loaded), the save path, a `dirty` flag, the Hub's last message, and the `--seed` for new games.

**App:** a stack of `Screen`s on the fixed-step loop. Each step the top screen gets the new keys and returns `Stay`, `Push`, `Pop`, `Replace` (pop then push) or `Quit`. Quit, or popping the last screen, ends the program with exit code 0. Only the top screen is drawn; dialogs are drawn by the screen that owns them. The canvas is 80x24, centered in larger terminals with the `bg` color around it.

**Widgets** (`src/app/widgets.hpp`, colors from ui.md's palette):
- `Menu`: vertical (↑/↓, `w`/`s`) or horizontal (←/→, `a`/`d`). Selection wraps and skips disabled items; Enter or `1`-`9` chooses an enabled item. The selected row shows `▶` in the accent color, disabled rows are dim, and an optional dim note follows the label.
- `TextInput` (printable ASCII up to a max length, blinking `_`), `NumberInput` (digits only, no more digits than the maximum has; Enter on an empty or out-of-range value shows "Enter a number from MIN to MAX" in the damage color).
- `MessageBox` (centered, at most 60 columns, word-wrapped, Enter or Esc dismisses), `ConfirmBox` (`Y`/`N`, Esc means No).
- `Typewriter`: 40 characters per second; the first Enter reveals the rest, the second finishes.
- Status panel: **rows 0-4** (ui.md says 0-3; see below). Row 1: name, path, level, HP bar (20 cells, colored by the >50% / >20% thresholds), MP bar (8 cells). Row 2: ATK/DEF/LIFE including equipment, EXP, Next, Gold. Row 3: `Weapon <name>[+N]   Armor <name>`, with `+N` only above upgrade 1 and `—` for an empty slot. Text is clipped at the border, never drawn over it.

**Screens so far:** Title (banner, New Game / Continue / Quit; Continue is disabled when no save file exists; hint row 22). Quit exits. New Game and Continue arrive with task 10.2.

**Spec drift:** ui.md's status panel puts gear on row 2 in rows 0-3, but with real item names that line needs about 84 columns, so gear moved to its own line and the panel uses rows 0-4. Row 4 was unused on every screen that shows the panel. `App` takes the terminal (as `LoopIo`), the renderer and the first screen instead of the `Session` from interfaces.md §14, since it never uses the session itself.
