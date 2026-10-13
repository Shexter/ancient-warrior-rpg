# Benchmarks

Measured numbers only. Each entry records the command, machine, and output.

## Simulator throughput (chunk 5)

Machine: Apple M4 Pro, 12 cores, macOS (Darwin 25.6), Apple clang 21, `release` preset.
Date: 2026-10-11.

```
./build/release/heroes-sim --path fighter --enemy medusa --battles 100000 --seed 7 --bench
battles: 100000  seed: 7  threads: 12  policy: attack
enemy: medusa  hero: fighter atk=100 def=25 life=0 hp=350 mp=50
win: 0.00%  loss: 100.00%  draw: 0.00%  fled: 0.00%
avg turns: 1.70  avg hp left on wins: 0.00
wall: 0.016 s  throughput: 6209701 battles/s
```

Single thread (`--threads 1`):

```
wall: 0.034 s  throughput: 2915052 battles/s
```

Note: a starting Fighter with the `attack` policy loses every battle in about 1.7 turns, so these runs are short battles. Longer builds (more HP, skills) will give lower battles/s.

## Terminal output: diff vs full redraw (chunks 8-9 demo, removed in task 10.1)

`heroes --demo-stats` renders the demo headlessly for 120 frames (80x24, truecolor) and compares the bytes written per frame by the diff renderer against a renderer that redraws everything every frame (a fresh `Renderer` each frame). The demo shows the 24x24 Fighter sprite and a moving 12x3 box.

```
frames: 120  full redraw: 5844 bytes/frame  diff: 160 bytes/frame (20 cells/frame)
```

The diff renderer writes under 3% of the bytes. Only the box moves (20 cells/s at 60 steps/s), so about one column changes per frame (20 cells). The sprite is static after the first frame. With only the box on screen (chunk 8) the same measurement was 2725 vs 219 bytes/frame.

## Avaritia balance band (reskin-act-one task 3.3, 2026-10-12)

`heroes-sim --enemy avaritia --battles 20000 --seed 7 --policy smart`, shipped data. Avaritia is 2200 HP / 160 ATK / 15 DEF / 5 LIFE with Medusa's fight pattern. The band (balance-simulator spec) is 40–80% for the mid-game build and under 5% for starting gear.

| path | Sword +5, Iron Shield +5, Guardian Angel + Righteous Strike | starting gear, no skills |
|---|---|---|
| Knight (`fighter`) | 42.31% | 0.00% |
| Hunter (`hunter`) | 61.51% | 0.00% |
| Pilgrim (`rogue`) | 48.60% | 0.00% |

Tuning notes:
- Only ATK moved from the planned 2200/150/15/5. At ATK 150 the Pilgrim won 82.84%, above the band.
- The Pilgrim's result has a cliff: at ATK 158 it wins 82.84%, at ATK 160 48.60%. With 100 max HP, one fewer enemy hit per fight decides it.
- The Knight sat at about 42% across every HP, DEF and LIFE value tried (2000–2400, 15–90, 5–40). Its ceiling here is set by its damage output, not the boss's numbers.
- These values are pinned by the `AvaritiaBalance` test (task 3.4).

## Terminal output at 120x40 (add-large-art-layout task 4.4, 2026-10-12)

`LargeLayout.DiffRenderingStillPaysOffAt120x40` uses the same method as above: bytes per frame from the persistent diff renderer against a fresh renderer every frame (a full redraw), truecolor. It runs over 120 frames (2 s) of a battle against Medusa, starting with an attack: the lunge, damage flash, floating number and log lines.

| canvas | full redraw | diff | diff share |
|---|---|---|---|
| 80x24 compact | 10,916 bytes/frame | 883 bytes/frame | 8.1% |
| 120x40 glyph sprites | 18,506 bytes/frame | 1,467 bytes/frame | 7.9% |
| 120x40 half-block sprites | 31,245 bytes/frame | 2,786 bytes/frame | 8.9% |

The large canvas has 2.5x the cells, but a glyph battle writes only 1.7x the bytes of a compact one, because glyph cells set only a foreground color. Half-blocks set two colors per cell and cost about 1.9x the glyph layout. The diff share is about the same at every size, since only the animated sprite region changes. These numbers are higher than the chunk 8 demo's 160 bytes/frame because a battle animation moves a whole sprite (the lunge) instead of a 12x3 box. The test fails if the diff renderer ever writes more than a quarter of a full redraw.

## Scripted vs native enemies (add-lua-scripting task 3.2, 2026-10-12)

Medusa and Avaritia now take their turns through `data/scripts/medusa.lua`. Each simulator worker owns its own `ScriptHost`. Native numbers were measured on the same machine, immediately before the C++ pattern was deleted. Release preset, seed 7, 200,000 battles.

| run | native | scripted | scripted / native |
|---|---|---|---|
| Medusa, starting Knight, `attack` policy, 1 thread | 2,341,146 battles/s | 886,889 battles/s | 38% |
| same, 8 threads | 11,203,985 battles/s | 4,574,003 battles/s | 41% |
| Avaritia, Hunter with Sword +5 / Iron Shield +5 and two skills, `smart`, 1 thread (4.3 turns) | 1,125,719 battles/s | 324,293 battles/s | 29% |
| same, 8 threads | 3,659,393 battles/s | 1,565,320 battles/s | 43% |

- Every run is above the 25% target, so the reference's §8 mitigation (caching function handles, reusing one ctx per battle) was not applied.
- The longest fights cost the most. The 29% single-thread case spends most of its time in script calls (about two enemy turns per battle).
- The results are identical to the native runs (same win rate, average turns and HP left), as the parity requirement demands.

## Cave win-chance estimates (fun-pass task 3.2, 2026-10-12)

`caveWinChance` for all 30 caves (200 fights each), `Estimate.CavesLeaveTheStateAndItsRngUntouched`, Apple M-series:

| build | all 30 caves |
|---|---|
| debug | 6.7 ms |
| ci (release) | 0.1 ms |

Computed once when the cave list opens and once per finished round (one cave), never per frame, so no cache is needed.

A fresh Knight's odds read 100 / 100 / 94 / 0 … for caves 1–4 onward: the jump from cave 3 to 4 is a real cliff in the training curve, not an estimator artifact.

## Act I boss bands (fun-pass task 4.3, 2026-10-13)

`heroes-sim --enemy BOSS --battles 20000 --seed 7`, shipped data, each path with the boss's intended build from the fun-pass reference (Guardian Angel and Righteous Strike only; Smite and Sanctuary arrive in chunk 5 and 5.2 re-runs this). The band: smart 50–80%, attack-only at least 30 points lower, smart with starting gear under 5%.

| boss (HP / ATK / DEF) | path | smart | attack | starting gear |
|---|---|---|---|---|
| Avaritia (3200 / 200 / 15) | Knight | 69.56% | 0.00% | 0.00% |
| | Hunter | 64.16% | 0.00% | 0.00% |
| | Pilgrim | 51.86% | 0.01% | 0.00% |
| Gula (4300 / 410 / 25) | Knight | 71.08% | 23.79% | 0.00% |
| | Hunter | 65.38% | 6.67% | 0.00% |
| | Pilgrim | 53.39% | 16.12% | 0.00% |
| Ira (25000 / 315 / 40) | Knight | 75.87% | 28.06% | 0.00% |
| | Hunter | 52.67% | 12.55% | 0.00% |
| | Pilgrim | 59.03% | 12.71% | 0.00% |

Tuning notes:
- As first written the three scripts drew no random numbers, so every build won 0% or 100%. With the user's approval (2026-10-13) each boss now rolls its telegraph (Avaritia 1 in 4, Gula 1 in 3, Ira 1 in 2 below half HP) instead of a fixed cadence, and every hit varies by ±15%.
- The reference starting stats were far below the intended builds' gear: Ira at 5000 HP died to two hits from a 3000-ATK hero. Ira needed 25000 HP so the fight lasts long enough for choices to matter.
- Results are cliffy in boss HP (one more or one fewer hero hit), and the Pilgrim (lowest HP) sets the lower edge for every boss.
- Pinned by `ActOneBalance.ShippedBossesStayInTheBand` on a 4,000-battle prefix.
