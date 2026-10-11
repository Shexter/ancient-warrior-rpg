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

## Terminal output: diff vs full redraw (chunk 8 demo)

`heroes --demo-stats` renders the moving-box demo headlessly for 120 frames (80x24, truecolor) and compares the bytes written per frame by the diff renderer against a renderer that redraws everything every frame (a fresh `Renderer` each frame).

```
frames: 120  full redraw: 2725 bytes/frame  diff: 219 bytes/frame (22 cells/frame)
```

The diff renderer writes about 8% of the bytes. The box moves 20 cells/s at 60 steps/s, so about one column changes per frame (22 cells). The full redraw figure is already small because runs of cells with the same colors share one color escape.
