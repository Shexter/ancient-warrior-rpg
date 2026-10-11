# Code Correctness Contract

This file defines the minimum engineering standard for implementing OpenSpec changes.

It applies to every OpenSpec implementation chunk and its review.

## 1. Correctness

Implementation must:

- satisfy the approved OpenSpec requirement;
- implement only the approved chunk;
- preserve existing behavior unless OpenSpec explicitly changes it;
- leave the project building, tests passing, and the game runnable.

Do not implement later chunks early.

## 2. Smallest Reasonable Change

Prefer the smallest change that correctly satisfies the requirement.

Before adding new code:

- search for existing equivalent behavior;
- reuse existing domain logic where appropriate;
- avoid duplicating game rules (damage, drops, leveling live in one place);
- avoid speculative abstractions;
- avoid unrelated refactors.

Do not add infrastructure for hypothetical future requirements.

## 3. Readability

Code should be understandable without reconstructing the entire system.

Prefer:

- descriptive names;
- clear control flow;
- focused responsibilities;
- explicit behavior.

Avoid:

- unnecessary nesting;
- unnecessary indirection;
- vague names;
- hidden state changes (mutable globals, out-parameters that silently change state);
- clever implementations that are difficult to trace.

## 4. Complexity

Complexity is a review signal, not an automatic failure.

Inspect functions with:

- deep nesting;
- many branches;
- difficult error paths;
- excessive boolean conditions;
- cyclomatic complexity around or above 10.

Refactor only when doing so makes responsibilities genuinely clearer.

Do not refactor solely to reduce a metric.

## 5. Security and Trust Boundaries

When relevant, verify:

- API keys are never committed, logged, written to save files, or compiled into the binary;
- LLM output is treated as untrusted text: it never executes, never changes game state directly, and is length-limited and sanitized of terminal escape sequences before display;
- mod/Lua scripts run sandboxed (no `os`, `io`, `require`, or `debug` access);
- save files and content JSON are validated; malformed input produces an error, not a crash or undefined behavior;
- network calls use TLS and time out.

Security regressions are blocking.

## 6. Performance

Check for obvious regressions such as:

- allocation inside the per-frame render or per-tick simulation loop;
- full-screen redraws where diff rendering applies;
- blocking network calls on the game loop thread;
- unnecessary copies of large containers.

Do not optimize without evidence (benchmark or profile), but do not introduce obvious regressions.

## 7. Tests

Every chunk must verify the behavior it introduces.

Use appropriate:

- unit tests;
- integration tests;
- failure-path tests;
- determinism tests (same seed and inputs produce the same result).

Existing relevant tests must continue to pass, under the sanitizer build when CI runs it.

Do not add meaningless tests solely to increase coverage.

## 8. Cleanup

Do not leave:

- commented-out code;
- debug logging;
- dead code;
- unused includes;
- temporary mocks;
- unexplained TODO/FIXME/HACK/TEMP markers.

Any deliberate follow-up must be reported.

## 9. Documentation

If implementation changes important:

- inputs;
- outputs;
- types;
- data flow;
- side effects;
- security behavior;

mark the relevant SimpleDocs documentation for update.

OpenSpec explains intended behavior.

SimpleDocs explains implemented behavior.

## 10. Review Standard

Before approving a chunk ask:

1. Does it satisfy OpenSpec?
2. Is it approximately the smallest reasonable solution?
3. Did it unnecessarily duplicate existing logic?
4. Is the important code easy to follow?
5. Is complexity justified?
6. Are trust boundaries (keys, LLM output, scripts, files) preserved?
7. Are obvious performance regressions avoided?
8. Are important behaviors tested?
9. Is temporary/dead code absent?
10. Could another developer safely modify this six months from now?

Findings are classified as:

- `BLOCKING` — must be fixed before approval.
- `RECOMMENDATION` — useful improvement but not required.
- `NOTE` — informational only.
