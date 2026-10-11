# RNG (`src/game/rng.{hpp,cpp}`)

- PCG32 (XSH-RR) seeded through SplitMix64. Output is bit-exact with the reference vectors in `tests/rng_test.cpp`.
- `Rng::stream(seed, index)`: an independent stream per `(seed, index)`.
- `below(n)` is unbiased (Lemire). `percent(p)` is `below(100) < p`.
- `fork()` consumes exactly 2 draws from the parent and returns a new stream. Every battle, training session, forge, auction and mission start forks once from `GameState::rng`, so each advances the master by exactly 2 draws no matter what happens inside.
- No `<random>`, `rand` or `std::random_device` in `src/game/`. A new game's seed is chosen by the caller.
- `Rng` is trivially copyable and has `==`, so tests snapshot and compare it.
