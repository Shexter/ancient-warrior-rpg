# Tasks

## 1. Portrait canvas and minimum size

- [ ] 1.1 Add the portrait canvas (40×32) and size rule (narrower than 80 and at least 40×32, or `--layout portrait`), and change the loop's minimum to "80×24 or 40×32" with the prompt naming both; verify loop and option tests for 45×40, 60×20, 100×30 and forced portrait
- [ ] 1.2 Add the portrait widgets (3-row status strip, numbered bottom menu with Back last, dialogs at most 36 wide); verify widget tests at 40×32
- [ ] 1.3 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 2. Portrait screens

- [ ] 2.1 Confirm the canvas size against the user's phone (`tput cols; tput lines`) and adjust the constant if needed
- [ ] 2.2 Portrait Title, Intro, Create Hero, Hub, Mission Select, Intro and Result; verify a 40×32 frame test for each (the scene on top, numbered menu rows below)
- [ ] 2.3 Portrait Battle (enemy sprite, bars, 3 log lines, actions as rows, Skill and Item overlays in the bottom half) and Training screens; verify frame tests and that a full battle can be played with number keys only
- [ ] 2.4 Portrait town screens (Almonry and Thesaurus one list at a time, Forge, Prior incl. Blessings); verify frame tests and a buy, sell and deposit by number keys only
- [ ] 2.5 Update SimpleDocs (`app.md`, `terminal.md`); verify against the screens
- [ ] 2.6 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 3. Tap to select

- [ ] 3.1 Enable SGR mouse reporting in the terminal and disable it on every restore path; decode presses into `Key::Click{x, y}`; verify input tests for click sequences and terminal tests for the restore bytes
- [ ] 3.2 Menus choose the row under a click (all layouts); verify a screen test tapping "Town" on the portrait Hub
- [ ] 3.3 Play on a real phone terminal (or a 45×40 pseudo-terminal) through Hub → Town → battle using taps only and number keys only; record what was checked
- [ ] 3.4 Complete chunk quality check against `docs/engineering/code-correctness.md`.
