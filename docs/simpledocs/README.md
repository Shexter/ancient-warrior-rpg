# SimpleDocs

What the code **actually does** today. OpenSpec (`openspec/`) says what it should do; if the two disagree, that is a bug or a recorded spec drift.

| doc | covers |
|---|---|
| [rng.md](rng.md) | `Rng`: seeding, forking, where randomness is allowed |
| [content.md](content.md) | `data/*.json`, the loader, error reporting |
| [hero-and-battle.md](hero-and-battle.md) | `Hero`, `Inventory`, `grantExp`, the `Battle` engine and its events |
| [town-and-training.md](town-and-training.md) | training, merchant, blacksmith, guildmaster, bank, missions |
| [save.md](save.md) | save file format, load validation, save location |
| [simulator.md](simulator.md) | `heroes-sim` |

Not yet implemented (no docs yet): terminal engine, app screens.
