# Balance simulator (`tools/sim`, binary `heroes-sim`)

`heroes-sim --path fighter --enemy medusa --battles 100000 --seed 7 [--threads N] [--policy attack|smart] [--weapon ID[:LV]] [--armor ID[:LV]] [--skills a,b] [--bonus-hp|mp|atk|def N] [--bench] [--data-dir PATH]`

- Battle `i` uses `Rng::stream(seed, i)`. Work is split into contiguous ranges and merged in thread order, so results are identical for any thread count. At most 256 workers run (never more than the battle count); the report still prints the requested `--threads`.
- Policies only choose valid actions (`attack`, or `smart` priority list). If the engine rejects one, the tool aborts: that is a policy bug.
- Bad input prints `heroes-sim: error: <message>` to stderr, exits 2 and runs no battles. `--bonus-*` values must be 0-1,000,000.
- Default data dir is the source tree's `data/`; override with `--data-dir` (there is no env var, deliberately).
- Measurements are in `docs/benchmarks.md`.
