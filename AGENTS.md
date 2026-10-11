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
6. stop for human approval.

Reference material: `DESIGN_SPEC.md` (mechanics extracted from the original game) and `heroes RPG/` (original 2021–22 source, read-only; do not build or modify it).
