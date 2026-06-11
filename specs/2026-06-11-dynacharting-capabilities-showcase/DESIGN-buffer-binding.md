# T0.2 spike — embassy buffer-ID binding & vector routing (ENC-518)

**Verdict: GO.** The inversion is achievable with a **small, additive** embassy change (no hot-path regression, no breaking changes). Scope for T3.3 (ENC-533) is confirmed and bounded; **no scope explosion**. The scalar-fan vector views (T4.3) are unblocked.

## What the code already gives us
- `Orchestrator.ApplyInstruction` (orchestrator.go:146) is recipe-agnostic: it consumes `SceneOutput{Routes, CompoundBuffers}` from `SceneCompiler.Compile` and wires `routeState`s + per-buffer compound states. The hot path (`routeSimple`/`routeCompound`) is generic.
- `Route` (scene.go:132) already models **simple** (Stride 0/4 → monotonic APPEND) and **compound** (Stride>4 → N subs join into one packed record) modes. candle6 OHLC already uses compound.
- `PackUpdateRange` (binary.go:51) exists — a fixed-offset write primitive is already at the packing layer.
- Instruction-file path (main.go:69 `instructionFile` → `ParseVisualizationHint` → `ApplyInstruction`) carries `subscriptions[{id,streamKey,field}]` + a free-form `visualization` hint. **The hint is the extension point.**

## The one gap
Buffer IDs are **owned by the recipe**, not specifiable from the instruction file (legacy path hardcodes `100+index`; candles recipe hardcodes `10100…`). For the inversion (manifests live in the showcase, embassy is a generic pump) we need the instruction file to **declare** the requestId→bufferId mapping. And **`FullMask`/`JoinSlotBit` are uint8 → max 8 compound join slots**, so wide vectors (a 20-level depth ladder) can't ride the compound path.

## Design (additive)
A new **generic, data-driven recipe `showcase-explicit-v1`** + a **third route mode**. Three write modes total:

1. **append** (exists, `routeSimple`) — time-series streams (line, candle-overlay, scalar TA). 4B APPEND at monotonic offset; buffer grows over time.
2. **compound** (exists, `routeCompound`) — packed multi-field records, **≤8 slots** (candle6). N subs → one buffer; emit when all slots fire.
3. **fixed** (NEW, `routeFixedUpdate`) — **current-state vectors** (depth ladder, profile snapshot, heatmap row). Each sub writes its value via `PackUpdateRange` at a binding-declared fixed offset into a pre-sized buffer; the geometry reads the whole buffer as the live snapshot, overwritten in place each tick. ~20 LOC, zero-alloc (mirrors `routeSimple`'s pooled pattern). **This is the unlock for wide vector views** — sidesteps the 8-slot compound limit entirely.

### Instruction-file hint schema (new recipe)
```json
"visualization": {
  "recipe": "showcase-explicit-v1",
  "bindings": [
    {"slot":"vwap",        "requestId":"r-vwap", "bufferId":500, "mode":"append"},
    {"slot":"depth.bid.0", "requestId":"r-d0",   "bufferId":600, "mode":"fixed", "offset":0},
    {"slot":"depth.bid.1", "requestId":"r-d1",   "bufferId":600, "mode":"fixed", "offset":4},
    {"slot":"ohlc.open",   "requestId":"r-o",    "bufferId":700, "mode":"compound", "stride":24, "intraOffset":4, "slotBit":0}
  ],
  "buffers": [
    {"bufferId":700, "stride":24, "fullMask":15,
     "staticFields":[{"intraOffset":0,"kind":"recordIndex"},{"intraOffset":20,"kind":"const","value":0.4}]}
  ]
}
```
`compileShowcaseExplicit(subs, hint)` builds `Routes` (mode→Stride/IntraRecordOffset/JoinSlotBit or fixed-offset) + `CompoundBuffers` (from `buffers`) + a minimal valid `DocumentJSON` (declares the buffers so the existing `SceneToCommands` guard passes; **the showcase ignores embassy's scene-init and applies its own catalog manifest**).

## Code touched (all additive, ≤4h → confirms T3.3 estimate)
- `internal/pipeline/recipe_showcase_explicit_v1.go` (new) + register in `SceneCompiler.Compile` switch.
- `scene.go`: extend `HintBinding` (`BufferID`, `Mode`, `Offset`, compound fields) + a `Buffers []HintBufferSpec` on `VisualizationHint`; extend `ParseVisualizationHint` validation.
- `orchestrator.go`: add fixed-offset fields to `routeState` + `routeFixedUpdate`; `RouteValue` dispatch becomes group≠nil→compound / fixed→fixedUpdate / else→simple. **`routeSimple`/`routeCompound` untouched** → hot-path bench unchanged.
- Tests: recipe compile test + fixed-offset route test; `BenchmarkOrchestratorRouteValue` must stay 0 allocs.

## Constraints / decisions surfaced
- **Wide atomic time-series records (>8 fields joined per record)** are NOT supported (uint8 mask). No planned view needs it — depth/profile/heatmap render as `fixed` snapshots. If ever needed, widen `FullMask`→uint32 (separate, deferred).
- **Showcase must ignore embassy's `scene-init`** and apply its own manifest (else double-`createBuffer` conflicts on shared IDs). Records into T0.5/T5.4 (showcase side).
- No sovereignty impact (mock data; same packing path).

## Effect on the plan
- T3.3 (ENC-533) scope CONFIRMED & bounded: generic recipe + `fixed` route mode. ~4h.
- T4.3 (depth ladder / market profile / footprint) UNBLOCKED via `fixed` mode (data emitted as a scalar-fan, one sub per level/bin).
- No new tickets required; T0.1 contract should codify the three modes + the hint schema above.
