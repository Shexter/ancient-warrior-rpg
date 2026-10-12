# Design

## Context

This is a greenfield repository. The prior art is the original Windows-only source in `heroes RPG/` and the summary in `DESIGN_SPEC.md`. Where those disagree with this change's `reference/content-tables.md`, the content tables win, because they come from a line-by-line reading of the original that corrected the summary.

The author's Mac has Apple clang 21 (C++23 `std::expected` verified) and no CMake yet. Three later changes depend on this one: `add-procedural-dungeons`, `add-lua-scripting`, and `add-llm-game-master`. All three need game logic that is UI-independent and reports what happened as data.

This change is planned in detail so that a different model can implement it chunk by chunk without design decisions of its own. The detail lives in `reference/`:

| file | contents |
|---|---|
| `reference/content-tables.md` | every number, name, and text string, with **[decided]** fixes to original bugs |
| `reference/interfaces.md` | conventions, source tree, CMake and CI, exact algorithms, type signatures, file formats |
| `reference/golden-vectors.md` | test vectors for RNG, leveling, battles, drops, auction, forge |
| `reference/battle_ref.py` | Python model of the battle rules that generated the battle vectors |
| `reference/ui.md` | screen layouts, key bindings, animation table, palette |

## Goals / Non-Goals

**Goals:**
- One seam between logic and presentation: logic takes actions and returns events, and everything else reads events.
- Bit-identical results on macOS, Linux, and Windows for the same seed.
- A renderer small enough to read in one sitting that measurably avoids redundant output.
- Every rule pinned by a test vector before the UI exists.

**Non-Goals:**
- Sound. The original `.wav` files are not ported, and `Genshin.mp3` is third-party copyrighted music that must never be committed.
- Accounts, login, and password handling (single-player game).
- Mouse input, Kitty or sixel graphics, localization, wide (CJK or emoji) glyphs.
- Missions 2 and 3 (shown as "Coming Soon").
- Balancing. The ported numbers ship as-is, and the simulator exists to measure them. Tuning is a later data-only change.

## Decisions

### D1. C++23, CMake ≥ 3.25, FetchContent with pinned URLs and hashes
- C++23 for `std::expected`. Avoid `std::print`, because GCC 13 on the Ubuntu 24.04 runner lacks it. Use `std::format`.
- Dependencies (nlohmann/json v3.12.0, GoogleTest v1.17.0) come in through CMake's built-in FetchContent, by release URL and SHA-256, marked `SYSTEM` so their warnings don't fail `-Werror`. stb_image is vendored at a pinned commit for the offline sprite tool only.
- *Alternatives:* vcpkg or Conan (an extra tool for two dependencies); C++20 (would need a hand-written `expected`).

### D2. Hand-written ANSI renderer with diff present
- The framebuffer is a grid of cells. `present()` emits only changed cells, coalesces cursor moves and color changes, and wraps each frame in synchronized-update escapes (ignored by terminals that lack them).
- A thin platform layer has one `.cpp` per OS. It is isolated so game and engine code never include OS headers.
- `PresentStats` makes the optimization measurable: bytes per frame versus a full redraw go into `docs/benchmarks.md`.
- *Alternatives:* FTXUI (a good library, but the renderer is the part that demonstrates skill); notcurses (Linux-centric).

### D3. Plain value types and registries, not an ECS
- Content is immutable and keyed by string ID. Runtime state is copyable value types with defaulted `operator==`, which makes "a failed action changes nothing" a one-line test.
- *Alternative:* EnTT. A battle has two combatants, so an ECS buys nothing here.

### D4. Actions in, events out
- `Battle::act()` returns `std::expected<std::vector<Event>, ActionError>`, where `Event` is a `std::variant` of small structs. The UI animates events, the simulator ignores them, and the narrator later summarizes them. Town actions return results or a `TownError`.
- *Alternative:* observer callbacks. They hide ordering and make replays and tests harder.

### D5. Own RNG (PCG32 + SplitMix64 + Lemire bounded)
- Standard-library distributions are implementation-defined, so they break cross-platform determinism.
- The master RNG is forked once per battle, training session, forge, or auction, so each consumes a fixed 2 draws from the master whatever happens inside.
- The simulator derives battle `i`'s stream from `(seed, i)`, making results independent of thread count.
- Golden vectors pin all of this.

### D6. No floating point in game logic
- Integer math with truncating division is exact and identical on every platform. Floating point is confined to animation timing.

### D7. Original-game bugs are fixed by decision, not ported
- `reference/content-tables.md` lists each one as **[decided]**: misaligned prices, the auction exploit, revive soft-lock, unconsumed upgrade materials, uncharged skill learning, placeholder skill formulas, inconsistent forge odds, and the soft-locked bank.

### D8. Sprites baked offline
- `sprite_bake` (stb_image) converts PNG to a small text `.sprite` format. The game has no image dependency.
- Placeholder sprites are hand-written until real art exists.

### D9. Saves: one JSON file, temp-then-rename, versioned
- RNG state is stored as hex strings. Loading never modifies the file.
- A per-OS user data directory holds the save, with an environment override for tests.

### D10. Screen stack in the app layer
- `App` runs the fixed-timestep loop over a stack of `Screen` objects that return transitions.
- A `BattleView` turns event lists into timed animations, so `Battle` stays synchronous and instant.
- *Alternative:* one large state machine `switch`. It's harder to test and extend.

## Implementation Handoff

Implementers should read only what their chunk needs:

| chunk | read |
|---|---|
| 1 Toolchain | `interfaces.md` §1–§3 |
| 2 RNG | `interfaces.md` §1, §4; `golden-vectors.md` (RNG table) |
| 3 Content | `interfaces.md` §1, §5; `content-tables.md` |
| 4 Combat | `interfaces.md` §6–§7; `golden-vectors.md`; `content-tables.md` (skills, statuses, Medusa) |
| 5 Simulator | `interfaces.md` §13 |
| 6 Progression and town | `interfaces.md` §6, §8, §9; `content-tables.md` (economy); `golden-vectors.md` (roll helpers) |
| 7 Save | `interfaces.md` §10 |
| 8 Terminal | `interfaces.md` §12 |
| 9 Sprites | `interfaces.md` §12 (sprite format); `ui.md` (sprite table) |
| 10 Screens | `interfaces.md` §11, §14; `ui.md` |

Chunks that get a closer review before approval, because mistakes there are subtle:
- Chunk 2 (bit-exact RNG).
- Chunk 4 (turn and status timing).
- Chunk 7 (atomic writes and Windows rename).
- Chunk 8 (signal-safe terminal restore).

If an implementer finds that a golden vector disagrees with the written rules, it stops and reports. It doesn't edit the vector to match its code.

## Risks / Trade-offs

- [Windows `rename` over an existing file] → A test runs on the Windows CI job. If it fails, fall back to `ReplaceFileW` behind `src/platform/`.
- [Terminal left in raw mode after a crash] → RAII guard plus async-signal-safe handlers plus `atexit`. Document `reset` as the manual fix.
- [Half-blocks look poor without truecolor] → 256-color fallback (spec'd).
- [Ported numbers are badly balanced] → Expected. The starting Fighter loses to Medusa 100% of the time, and Supernova one-shots her at 300 ATK. The simulator measures this, and values tune in data later.
- [Implementer drifts from the reference] → Golden vectors fail loudly. The apply guidance requires reporting spec drift instead of silently reconciling it.
