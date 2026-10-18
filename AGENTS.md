# AGENTS.md

## OpenSpec, Engineering Quality, and SimpleDocs

OpenSpec is the source of truth for intended behavior:
`openspec/`

All OpenSpec implementation and review must follow:
`docs/engineering/code-correctness.md`

SimpleDocs explains actual implemented behavior:
`docs/simpledocs/`

When applying an OpenSpec change:

1. read the relevant OpenSpec change;
2. read `docs/engineering/code-correctness.md`;
3. implement only the approved chunk;
4. verify the chunk;
5. review against OpenSpec and the correctness contract;
6. stop for human approval (except under gnhf, see below).

Reference material: `DESIGN_SPEC.md` (mechanics extracted from the original game) and `heroes RPG/` (original 2021–22 source, read-only; do not build or modify it).

## gnhf (autonomous runs) — takes priority

When running under `gnhf` (a `gnhf/` branch or a `.gnhf/runs/` directory exists), these rules override the approval and cadence rules above:

- Do not stop for human approval. Review happens when the human merges the gnhf branch.
- One iteration = one unticked task from the OpenSpec `tasks.md` files (`fun-pass` first, then `add-procedural-dungeons`). Implement only that task, tick it, and commit.
- Verify locally (build + full test suite) before finishing; a failing iteration must leave nothing behind.
- Do not push, open PRs, or touch CI; the human pushes once per chunk after review.
- Skip tasks that require manual playtesting ("verify manually") and record them in `notes.md` instead.
- All other rules still apply: follow `docs/engineering/code-correctness.md`, update `docs/simpledocs/` if behavior changes, never modify `heroes RPG/`.
