# Tasks

## 1. Tap to select (primary input on phones)

- [ ] 1.1 Enable SGR mouse reporting in the terminal and disable it on every restore path; decode presses into `Key::Click{x, y}`; verify input tests for click sequences and terminal tests for the restore bytes
- [ ] 1.2 Menus choose the row under a click, and dialogs gain tappable `[Yes]`/`[No]`/`[OK]` (all layouts); verify screen tests tapping a Hub row and answering a ConfirmBox by tap
- [ ] 1.3 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 2. Portrait canvas and minimum size

- [ ] 2.1 Add the portrait canvas (40×32) and size rule (narrower than 80 and at least 40×32, or `--layout portrait`), and change the loop's minimum to "80×24 or 40×32" with the prompt naming both; verify loop and option tests for 45×40, 60×20, 100×30 and forced portrait
- [ ] 2.2 Add the portrait widgets (3-row status strip, numbered bottom menu with Back last, dialogs at most 36 wide); verify widget tests at 40×32
- [ ] 2.3 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 3. Portrait screens

- [ ] 3.1 Confirm the canvas size against the user's phone with the keyboard hidden (`tput cols; tput lines`; screenshots suggest about 48×48) and adjust the constant if needed
- [ ] 3.2 Portrait Title, Intro, Create Hero, Hub, Mission Select, Intro and Result; verify a 40×32 frame test for each (the scene on top, numbered menu rows below)
- [ ] 3.3 Portrait Battle (enemy sprite, bars, 3 log lines, actions as rows, Skill and Item overlays in the bottom half) and Training screens; verify frame tests and that a full battle can be played with taps only and number keys only
- [ ] 3.4 Portrait town screens (Almonry and Thesaurus one list at a time, Forge, Prior incl. Blessings); verify frame tests and a buy, sell and deposit by taps only and number keys only
- [ ] 3.5 Update SimpleDocs (`app.md`, `terminal.md`); verify against the screens
- [ ] 3.6 Play on the user's phone (or a 48×48 pseudo-terminal) from Hub through Town and a battle by taps only with the keyboard hidden; record what was checked
- [ ] 3.7 Complete chunk quality check against `docs/engineering/code-correctness.md`.

