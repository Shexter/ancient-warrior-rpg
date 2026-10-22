# add-glyph-renderer notes

## 2026-10-21: hero models approved (task 3.2)

The user approved the turntable contact sheet: "Fighter = Knight, Rogue = Rogue, Hunter = Rogue_Hooded with the crossbow. Recolour to the path palettes, and keep battle sprites for monsters for now."

- Recoloured: Hunter's teal cloth to green (hue 137°), Pilgrim's to purple (hue 275°). Knight's crimson cape already matches the Fighter sprite, so it's unchanged.
- The files are about 3.7 MB each, mostly KayKit's 76 animation clips. Only four clips are used; trimming the rest would shrink them if size matters.
- Looks dim at game scale, with grey armour reading as grey. Lighting is tuned in 3.3, where the real renderer is built.

## 2026-10-22: tasks 3.3–3.8 run without a stop for approval (the user's /goal)

- **3.6 playtest: not done, needs the user.** A 5 s smoke run of `heroes-window` starts cleanly with the three models loaded (one log line, the example mod). Walking every landmark, a field fight, a mission and back, and closing with unsaved changes still need a person at the window. Things to look at in that playtest:
  - the hero is about two tiles tall and reads mostly grey (the Knight's armour); the glyphs are small at 720p;
  - walk plays at about 3.4× clip speed at the field pace (0.45 clip seconds per tile), which may look like a jog;
  - the Hunter and Pilgrim use the one-handed chop for every attack, crossbow included.
- **Found and fixed on the way (pre-existing, caught by UBSan in the asan display tests):**
  - `HubScreen` read `portrait_` while building its menu, before the member was initialised (declaration order). It's now declared before `menu_`.
  - `heroes-sim --items` with an entry missing its colon formed a pointer past the string before checking for the colon.
- **The display machine:** `window-display` tests segfault inside `InitWindow` when the Mac's display is asleep (GLFW finds no monitor). `caffeinate -u -t 2` before the run wakes it.
- **Quality check (3.8)** against `docs/engineering/code-correctness.md`:
  - *Scope:* hero models only; monsters keep their sprites, as approved. No World changes: attack and hit are read from cooldowns and damage numbers in the scene view.
  - *Purity:* `glyphsFromPixels` and `poseOf` are pure and tested; the World is never written (the untouched test reruns with a model).
  - *Errors:* a missing model is skipped; an unloadable file, a missing clip or unskinned meshes exit 2 naming the file. Error paths free what they loaded; the unskinned-mesh throw leaks the other files' animations, marked `ponytail:` (the process exits).
  - *Per-frame cost:* one `LoadImageFromTexture` allocation per frame for the hero's readback, marked `ponytail:`; 0.3–0.7 ms a frame measured (docs/benchmarks.md).
  - *Builds:* clang debug, asan (with UBSan, display tests included) and ci 593/593; window-off 558/558; GCC 14 Release `-Werror`; mingw syntax check clean apart from raymath's own warnings when included without `-isystem`.
