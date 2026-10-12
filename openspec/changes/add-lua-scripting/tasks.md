# Tasks

The reference is `reference/lua-interfaces.md` (abbreviated "ref" below).

## 1. Lua Runtime and Sandbox

- [ ] 1.1 Add Lua 5.4.7 and sol2 v3.5.0 to `cmake/Dependencies.cmake` with the URLs and hashes in ref §1, build `lua_static`, and link `heroes_game`; verify all CI jobs build and a test runs `return 1 + 1` through sol2
- [x] 1.2 Implement `ScriptHost` (allowlist, frozen `_G`, per-script environments, chunk names, count hook, 8 MiB allocator) per ref §3; verify `tests/script_sandbox_test.cpp`: `io.open`, `os.time`, `require`, `load`, `math.random`, `debug`, and a global assignment each fail; `while true do end` and a 100 MB string build both stop with errors; the enumerated reachable globals equal the allowlist exactly; a syntax error message contains the file path and line
- [ ] 1.3 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 2. Battle API and Fallbacks

- [ ] 2.1 Add the optional `script` field to enemies and skills (exactly one of `effect` or `script` for skills), script-shape validation at load, `ev::ScriptSaid`, `ev::ScriptError`, and `ActionError::ScriptFailed`, and teach `BattleView` to log the two new events (the said text, and "Something went wrong with <name>'s magic." for an error); verify content validation tests for both-effect-and-script, a missing script file, and a script not returning the required table, plus BattleView tests for both log lines
- [ ] 2.2 Implement `BattleApi` (every ctx member in ref §4, including integer checks, range checks, and no-ops after the battle ends) and wire scripted enemy turns and scripted skills into `Battle`; verify `tests/script_battle_test.cpp`: the scripted-skill 600-damage scenario; a float argument is rejected as a script error; ops after a death are no-ops
- [ ] 2.3 Implement transactional failure per ref §5; verify tests: an infinite-loop enemy script produces `ScriptError` then a basic attack with no other events from the failed script; a skill script that damages then errors leaves HP, MP, RNG, and events unchanged and returns `ScriptFailed`
- [ ] 2.4 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 3. Port Medusa

- [ ] 3.1 Add `data/scripts/medusa.lua` (ref §4), point Medusa and Avaritia (added by `reskin-act-one` with Medusa's pattern; and Gorgon Spawn, if dungeons have shipped) at it, and delete `EnemyBehavior::MedusaPattern` and its C++ code; verify core golden scenarios A–D and the `AvaritiaBalance` tests pass unchanged on all three CI platforms
- [ ] 3.2 Give each simulator worker its own `ScriptHost`; verify the 1-vs-8-thread determinism test still passes, and record native (pre-change, from `docs/benchmarks.md`) vs scripted Medusa throughput; if below 25% of native, apply the ref §8 mitigation and re-measure
- [ ] 3.3 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 4. Mods

- [ ] 4.1 Implement mod discovery, manifest validation, path containment, priority merge, stderr override and loaded lines, and whole-mod skip with re-merge per ref §6; verify `tests/mods_test.cpp` with fixture mod folders: add-enemy, two-mod override (priority wins, both logged), broken mod skipped while others load, and a `../` script path rejected
- [ ] 4.2 Add active mods to saves with a version bump and the missing-mod ConfirmBox; verify tests: an old-version save loads with no mods; a save naming a missing mod reports it and leaves the file byte-identical
- [ ] 4.3 Add `mods/example-cyclops/` per ref §7; verify manually: the Cyclops Cave mission appears at level 15 (edit the save to reach it), the boulder line appears on turns 3 and 6, and Boulder Toss can be learned and cast
- [ ] 4.4 Complete chunk quality check against `docs/engineering/code-correctness.md`.

## 5. Final Verification

- [ ] 5.1 Verify CI is green with asan (sandbox and parity tests included) and `openspec validate add-lua-scripting --strict` passes; list any SPEC DRIFT
- [ ] 5.2 Complete chunk quality check against `docs/engineering/code-correctness.md`.
