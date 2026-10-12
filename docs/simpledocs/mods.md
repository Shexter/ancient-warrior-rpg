# Mods (`mods/`, `loadContent(dataDir, modsDir, log)`)

**Where:** `HEROES_MODS_DIR`, else `mods/` beside the data folder. A missing folder means no mods. Each subfolder with a `mod.json` is a mod; other folders are ignored. The repo ships one, `mods/example-cyclops/`, which is active in normal play.

**`mod.json`:** `{"name", "version", "priority"?, "description"?}`.
- `name` matches `[a-z0-9-]`, is 1-40 characters, and equals the folder name.
- `version` is any string of 1-20 characters.
- `priority` is a whole number, default 0. Unknown keys are errors.

**What a mod can contain:**
- Any of `items.json`, `skills.json`, `enemies.json`, `missions.json`, `recipes.json`, in the base formats, and `scripts/`.
- `paths.json`, `story.json` and `dungeon.json` are not allowed.
- Script paths are relative to the mod folder and must stay inside it. They are named `mods/<name>/<path>` in errors and in `Content::scripts()`.
- Enemy sprites come from the base `assets/sprites/`.

**Merging:**
- Mods apply after the base in (priority, folder name) order.
- An entry whose ID already exists replaces it whole, in place, so drop-table order is kept. A new ID is added at the end.
- Validation runs on the merged content: the same rules as the base, including script sandbox loading.

**Bad mods:** a mod is skipped whole when:
- it can't be read (manifest, JSON, unknown keys, a forbidden file, a script path leaving the folder); or
- an error in the merged content is about an entry it supplied. Each error is blamed on whichever mod provided that entry, and the merge is redone without the blamed mods.

The base content is validated alone first; if it fails, startup fails with exit 2 as before.

**Startup log** (stderr, before the terminal changes):
- `mod '<name>' skipped: mods/<name>/<file>: <entry>: <message>`, plus ` (and N more)`;
- `mod '<name>': overrides <file>/<id>`;
- `mod '<name>' <version> loaded`.

**Saves:**
- Save version 2 records `"mods": [{"name", "version"}]` for the mods active when saving. Version 1 saves load with none.
- On Continue, if a recorded mod isn't loaded now (missing, or another version), the Title asks "This save used mods that are missing: <list>. Continue anyway?". No leaves the file untouched.
- If Yes and the save then fails because it uses that mod's content, the message names the mods.

**Example mod:**
- `cyclops`: 2500 HP / 300 ATK / 60 DEF. Its script throws a boulder for double damage on every third turn, with the line "The Cyclops hurls a boulder!".
- The `cyclops_cave` mission: level 15, repeatable.
- The `boulder_toss` skill: 30 Fervor, learned at level 10 for 8000 gold and a Rosary. It deals 1.5x ATK through armor via its script.
