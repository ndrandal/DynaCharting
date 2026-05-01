# SPEC: Refactor Renderer.cpp + CommandProcessor.cpp

**Slug:** renderer-cmdproc-refactor · **Date:** 2026-05-01 · **Status:** Approved
**Repo:** DynaCharting · **Linear:** ENC-90

## Problem
- `core/src/Renderer.cpp` — 1587 LOC monolithic GL dispatch. One file
  handles every pipeline type; every new pipeline kind grows it
  further.
- `core/src/CommandProcessor.cpp` — 1379 LOC. Every JSON command kind
  is handled inline in a giant switch; adding a command means editing
  the central file.
Both are growing test/extend friction proportional to how many features
ship. Ahead of the GLFW window milestone (ENC-91) and any new pipeline
adds, splitting them up unblocks parallel work and reduces blast
radius.

## Proposed change
- **Renderer**: split per pipeline kind (`tri`, `line`, `points`,
  `instanced`, `text`) into separate `IRendererBackend` implementations
  in `core/src/render/backends/`. The orchestrating `Renderer` becomes
  a small dispatcher keyed on pipeline-kind enum.
- **CommandProcessor**: switch from a switch-on-cmd-kind to a
  registry-of-handlers pattern. Each command kind registers a handler
  function (`registerHandler("createBuffer", …)`) at startup. Adding a
  command becomes adding a file, not editing the central one.

## Scope
- New header `core/include/dc/render/IRendererBackend.hpp` + concrete
  backends per pipeline kind under `core/src/render/backends/`.
- `Renderer.cpp` shrinks to a dispatcher (~150 LOC).
- New header `core/include/dc/cmd/CommandHandlerRegistry.hpp` mirroring
  GMA's NodeTypeRegistry pattern from gma_v3.
- `CommandProcessor.cpp` shrinks to a dispatcher; each command kind
  gets its own file under `core/src/cmd/handlers/`.
- New tests:
  - `core/tests/test_renderer_dispatch.cpp` — wires a mock backend per
    kind, asserts dispatch picks the right one.
  - `core/tests/test_cmd_registry.cpp` — register, lookup, missing,
    duplicate.
- The 199 existing tests must pass without modification beyond
  include paths.

## Non-goals
- **No public API breaks.** `Renderer` and `CommandProcessor` retain
  their current signatures; only internals move.
- **No new pipeline kinds.** This ticket is structural.
- **No command additions.** Same.
- **Not restructuring the GL backend (`dc_gl`)** itself — only the
  dispatcher layer.

## Acceptance criteria
1. `core/src/Renderer.cpp` ≤ 200 LOC after refactor.
2. `core/src/CommandProcessor.cpp` ≤ 250 LOC after refactor.
3. `IRendererBackend` has at least 5 concrete implementations (one per
   pipeline kind currently supported).
4. CommandHandlerRegistry has at least one handler registered per
   existing command kind in `CommandProcessor`'s switch table.
5. All 199 existing C++ tests pass on Linux + macOS without source
   modifications beyond include paths.
6. Adding a new pipeline kind in a follow-up requires no edits to
   `Renderer.cpp` (verified by writing one in this PR as a sample,
   even if it's a no-op).

## Constraints
- **Performance:** registry dispatch adds one indirection per command;
  measure with the existing benchmarks if any. Acceptable bound: <2%
  slowdown on the hot dispatch path.
- **Compatibility:** internal-only refactor; downstream consumers
  unaffected.
- **Dependencies:** none.

## Affected systems
- `core/src/Renderer.cpp`
- `core/src/CommandProcessor.cpp`
- new `core/include/dc/render/IRendererBackend.hpp`
- new `core/src/render/backends/*`
- new `core/include/dc/cmd/CommandHandlerRegistry.hpp`
- new `core/src/cmd/handlers/*`
- new tests `test_renderer_dispatch.cpp`, `test_cmd_registry.cpp`

## Alternatives considered
- **Split each file in two but keep the same architecture** —
  rejected, doesn't actually solve the central-edit problem.
- **Visitor pattern instead of registry** — possible but registry
  matches the gma_v3 conventions we just shipped.

## Risks
- Refactor introduces a subtle behavior diff. Mitigation: 199 existing
  tests + mandate `git diff` review per backend.

## Open questions
- None.
