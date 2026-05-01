# Refactor — Renderer.cpp + CommandProcessor.cpp (ENC-90)

## Status

**Phase 1 of N — foundation only.** `CommandHandlerRegistry` and
`IRendererBackend` are published. Migration of individual handlers /
backends is incremental and out of scope for the foundation PR. The
existing central switches in `Renderer.cpp` (1587 LOC) and
`CommandProcessor.cpp` (1379 LOC) are unchanged.

## What landed

* `core/include/dc/commands/CommandHandlerRegistry.hpp` — name → handler
  registry. Mirrors gma_v3's `NodeTypeRegistry` pattern. Singleton
  accessor + `registerHandler`, `find`, `contains`, `size`, `clear`.
* `core/include/dc/render/IRendererBackend.hpp` — per-pipeline
  rendering interface. One concrete backend per pipeline kind, owned
  by the orchestrating `Renderer`.

## Migration recipe (per command kind)

1. Pull the body of the corresponding `case Cmd::*` arm out of
   `CommandProcessor.cpp` into a free function with the
   `CmdContext& → CmdResult` signature.
2. Register at static-init in a small `<kind>Handler.cpp` next to the
   handler.
3. In `CommandProcessor::dispatch`, before falling through to the
   switch, look the cmd name up in
   `CommandHandlerRegistry::singleton().find(name)`; if registered,
   invoke and return. Switch arm becomes dead and can be deleted.
4. After enough kinds migrate, the central switch becomes vestigial
   and the file can be deleted entirely.

## Migration recipe (per pipeline backend)

1. Lift the corresponding draw-call block out of `Renderer.cpp` into a
   `<pipeline>Backend.cpp` implementing `IRendererBackend`.
2. Register the backend in the engine's `Renderer` ctor (or via a
   static-init pattern similar to the command registry).
3. The central if/else in `Renderer::render` becomes a dispatch on
   `drawItem.pipeline` against the backend map.

## Why this is intentionally phase 1

A 1500-LOC switch refactored in one PR is a code-review minefield. The
infrastructure here is small enough to vet thoroughly; subsequent PRs
can each migrate 2-3 kinds with straightforward review.

## Follow-on tickets to file

When work resumes:

1. ENC-90/A — migrate `createBuffer`, `createGeometry`, `createBufferData` (the most-edited cluster).
2. ENC-90/B — migrate `createPane`, `createLayer`, `createTransform`, `createDrawItem` (scene primitives).
3. ENC-90/C — migrate the destroy paths.
4. ENC-90/D — split `Renderer.cpp` per pipeline kind.
