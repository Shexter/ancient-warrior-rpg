# SimpleDocs

What the code **actually does** today. OpenSpec (`openspec/`) says what it should do; if the two disagree, that is a bug or a recorded spec drift.

| doc | covers |
|---|---|
| [rng.md](rng.md) | `Rng`: seeding, forking, where randomness is allowed |
| [content.md](content.md) | `data/*.json`, the loader, error reporting |
| [hero-and-battle.md](hero-and-battle.md) | `Hero`, `Inventory`, `grantExp`, the `Battle` engine and its events |
| [town-and-training.md](town-and-training.md) | training, merchant, blacksmith, guildmaster, bank, missions |
| [save.md](save.md) | save file format, load validation, save location |
| [terminal.md](terminal.md) | framebuffer, renderer, input, loop, terminal |
| [sprites.md](sprites.md) | sprite format, drawing, tints, sprite_bake |
| [simulator.md](simulator.md) | `heroes-sim` |
| [app.md](app.md) | command line, screen stack, widgets, screens implemented so far |
| [scripting.md](scripting.md) | Lua sandbox: ScriptHost, allowlist, budget, memory cap |
| [mods.md](mods.md) | mods folder, manifests, merging, skipped mods, saves recording mods |
