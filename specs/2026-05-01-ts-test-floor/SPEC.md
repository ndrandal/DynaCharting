# SPEC: TS-side test floor (engine-host + chart-controller)

**Slug:** ts-test-floor · **Date:** 2026-05-01 · **Status:** Approved
**Repo:** DynaCharting · **Linear:** ENC-89

## Problem
The C++ core has 199 regression tests; the TypeScript prototype layer
(`packages/engine-host`, `packages/chart-controller`,
`apps/demos/hello-engine`) has zero. `EngineHost.ts` alone is 1494
LOC of largely uncovered GL dispatch / pipeline routing / buffer
management. Any TS-side change ships without a regression gate.

## Proposed change
Add vitest as the TS test runner. Cover the pieces that translate
between protocol bytes and rendered output (where bugs are most likely
silent): `CoreIngestStub` binary parser, `GlyphAtlas` packing, the
pipeline-spec dispatch in `EngineHost`. Pair the work with a small
refactor of `EngineHost.ts` into testable chunks (extract the
PipelineDispatcher and BufferManager classes — pre-work for ENC-90's
C++ refactor analog).

## Scope
- Root vitest config, `pnpm test` script.
- Tests:
  - `packages/engine-host/__tests__/CoreIngestStub.test.ts` — fuzz the
    binary record format (`[1B op][4B id][4B offset][4B bytes][payload]`)
    with valid + truncated + over-large inputs.
  - `packages/engine-host/__tests__/GlyphAtlas.test.ts` — shelf
    packing invariants, eviction, atlas overflow.
  - `packages/engine-host/__tests__/PipelineDispatcher.test.ts` —
    pipeline-spec → draw call mapping for `triSolid`, `line2d`,
    `points`, `instancedRect`, `instancedCandle`, `textSDF`.
  - `packages/chart-controller/__tests__/recipe.test.ts` — recipe
    create/dispose ID determinism, command-pair generation.
- Refactor (minimum viable for testability):
  - Extract `PipelineDispatcher` from `EngineHost.ts` (currently
    inlined).
  - Extract `BufferManager` (the per-buffer cap / eviction logic).
- Coverage target: 50% lines on `packages/engine-host` + 60% on
  `chart-controller`.

## Non-goals
- **Not testing actual WebGL2 rendering.** Mocks at the GL boundary;
  visual correctness stays under the C++ trial system.
- **Not adding e2e Vite tests.** `apps/demos/hello-engine` stays as a
  manual smoke for now.
- **Not landing the full `EngineHost` refactor.** Just enough
  extraction to make the dispatcher and buffer manager testable.
- **Not replacing the C++ trial visual audits** (`docs/trials/`).

## Acceptance criteria
1. `pnpm test` runs all TS tests from a clean clone.
2. Coverage ≥ 50% on engine-host, ≥ 60% on chart-controller (vitest
   `--coverage`).
3. `EngineHost.ts` shrinks to ≤ 1100 LOC (currently 1494) after the
   minimum extractions.
4. `PipelineDispatcher` and `BufferManager` are each their own file,
   exported from `engine-host/src/index.ts`.
5. Binary-parse fuzz test runs ≥ 1000 random inputs in ≤ 1s.

## Constraints
- **Performance:** test suite under 5s wall-clock.
- **Compatibility:** the extractions preserve `EngineHost`'s public
  surface — only internals move. Apps using `engine-host` don't
  recompile.
- **Dependencies:** none.

## Affected systems
- Root tooling: vitest config, `pnpm test`
- `packages/engine-host/src/EngineHost.ts` (refactor)
- new files: `PipelineDispatcher.ts`, `BufferManager.ts`
- `packages/engine-host/__tests__/`, `packages/chart-controller/__tests__/`

## Alternatives considered
- **Headless WebGL via puppeteer** — too heavy for v1; mock GL is
  enough.
- **Wait for the full `EngineHost` refactor before adding tests** —
  no; the goal here is ratchet quality up *as* we refactor.

## Risks
- Extraction surfaces preexisting bugs. Acceptable — they were always
  there; we just gain a place to fix them.

## Open questions
- None.
