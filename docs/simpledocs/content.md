# Content (`data/*.json`, `src/game/content*.{hpp,cpp}`)

Seven files: `paths`, `items`, `skills`, `enemies`, `missions`, `recipes`, `story`. Keys are camelCase; enums are snake_case strings. `data/` reproduces `reference/content-tables.md`.

`loadContent(dataDir)` returns `std::expected<Content, std::vector<ContentError>>`.
- It reads every file and **collects all errors** before failing: malformed JSON (with line/column), unknown or missing keys, duplicate or badly formed ids, out-of-range numbers, unknown references, kind mismatches, missing `fighter`/`hunter`/`rogue`.
- Cross-reference checks only run when every file parsed.
- Enemy sprites are looked up in `<dataDir>/../assets/sprites/`.
- `formatError` renders `file: entry: message`.

`Content` is immutable after loading. `item(id)` and friends assume the id exists; `findItem(id)` returns `nullptr` instead. Lists keep file order, which `rollDrop` depends on.
