# SPEC: Audit + document the BindingEvaluator commit

**Slug:** binding-evaluator-audit · **Date:** 2026-05-01 · **Status:** Approved
**Repo:** DynaCharting · **Linear:** ENC-88

## Problem
Commit `0b8b7d8` ("dump :(") landed `BindingEvaluator` (~330 LOC),
SceneDocument extensions, and three new tests (`d80_1`, `d80_2`,
`d80_3`) without a SPEC, PLAN, or descriptive commit message. The
binding feature ships in master but has no design record explaining
intent, surface, or stability commitments. Future contributors can't
tell whether it's experimental, partially implemented, or production-
ready.

## Proposed change
Treat the existing code as ground truth and write a **retroactive**
SPEC that documents what BindingEvaluator does, what its public
surface is, what invariants the tests pin, and which parts (if any)
are intentionally provisional. Either declare it stable (no API
prefix) or wrap the public surface in an `EXP_` prefix until the
design is finalized.

## Scope
- New `docs/binding-evaluator.md` (or this SPEC reformatted) with:
  - One-paragraph summary of what bindings do in the scene model.
  - Full public-API enumeration (header signatures, json schema for
    binding entries, lifecycle hooks).
  - List of invariants pinned by `d80_1` / `d80_2` / `d80_3`.
  - Performance characteristics (eval cost, allocation pattern).
  - Known limitations / things the author knew weren't done.
- A short audit pass: read every site that calls into
  `BindingEvaluator`; flag any TODO/FIXME/HACK comments and either
  resolve them or add them to a follow-up ticket.
- Decision: stable or `EXP_`-prefixed?
  - Default: declare **stable** (existing tests cover behavior,
    nothing in the audit suggests partial implementation).
  - If the author has reservations, switch to `EXP_` prefix on the
    public types and gate behind a CMake flag.

## Non-goals
- **Not redesigning the binding system.** Pure documentation pass.
- **Not adding new binding kinds.** Future-feature work.
- **Not changing test coverage.** `d80_*` tests already cover the
  shipped surface.
- **Not retroactively rewriting the commit message** — git history is
  immutable; the SPEC is the substitute.

## Acceptance criteria
1. `docs/binding-evaluator.md` exists with the sections listed above.
2. `BindingEvaluator`'s public methods each have a Doxygen-style
   comment naming their precondition, postcondition, and any allocation
   it performs.
3. Every TODO/FIXME/HACK in the binding-related files is either
   resolved in this same PR or filed as a separate Linear ticket and
   the comment updated to reference it.
4. README or ROADMAP.md gains a one-line "Binding system: see
   `docs/binding-evaluator.md`" pointer.
5. If `EXP_` prefix is chosen, every public binding type carries it
   and a comment explaining when the prefix gets removed.

## Constraints
- **Performance:** none — documentation work.
- **Compatibility:** if `EXP_` prefix is added, downstream consumers
  must rename. If "stable" is chosen, no impact.

## Affected systems
- `core/src/binding/*` (read-only audit)
- new `docs/binding-evaluator.md`
- `README.md` or `ROADMAP.md` pointer line

## Alternatives considered
- **Declare it experimental and gate it off** — over-cautious; the
  feature has tests and is in master. Pick this only if the author
  reviewing flags real concerns.
- **Leave it undocumented** — rejected; failure to communicate intent
  on a 330-LOC subsystem is exactly the kind of drift that bit
  contract-audit ENC-41/50 in gma_v3.

## Risks
- Documenting incorrectly entrenches a wrong understanding. Mitigation:
  the SPEC is reviewed by whoever wrote the original code (presumed
  to be the user) before merge.

## Open questions
- Was this commit intended for master or experimental? Resolved during
  the audit conversation.
