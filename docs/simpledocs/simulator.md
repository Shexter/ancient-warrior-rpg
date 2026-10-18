# Balance simulator (`tools/sim`, binary `heroes-sim`)

`heroes-sim --path fighter --enemy medusa --battles 100000 --seed 7 [--threads N] [--policy attack|smart] [--weapon ID[:LV]] [--armor ID[:LV]] [--skills a,b] [--bonus-hp|mp|atk|def N] [--blessings N] [--items id:count,...] [--bench] [--data-dir PATH]`

- Each worker thread owns a `ScriptHost` for scripted enemies (Lua states aren't thread-safe). Battle `i` uses `Rng::stream(seed, i)`. Work is split into contiguous ranges and merged in thread order, so results are identical for any thread count. At most 256 workers run (never more than the battle count); the report still prints the requested `--threads`.
- Policies only choose valid actions. `attack` always attacks. `smart` is `game::smartAction` (`src/game/autoplay`), shared with the game's win-chance estimates: when the enemy is Charging, Smite if affordable, otherwise defend; below 30% HP heal with Angel Wings if affordable, then Sanctuary (unless already Guarding), otherwise with the stocked item that restores the most HP; then Judgment on an enemy below 25% HP; then the strongest ready damage skill; otherwise attack. `--items` gives every battle that consumable stock (consumables only, 1-99 each). If the engine rejects one, the tool aborts: that is a policy bug.
- Bad input prints `heroes-sim: error: <message>` to stderr, exits 2 and runs no battles. `--bonus-*` and `--blessings` values must be 0-1,000,000. `--blessings N` spends N points with the game's `blessing` rule, round-robin over Max HP, Max Fervor, ATK, DEF (so remainders go to HP first), on top of any `--bonus-*`.
- Default data dir is the source tree's `data/`; override with `--data-dir` (there is no env var, deliberately).
- Measurements are in `docs/benchmarks.md`.
