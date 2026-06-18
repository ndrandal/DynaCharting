# SPEC: DynaCharting — Interaction Layer (signals · per-instance pick · data-bound transitions)

**Slug:** interaction-layer · **Date:** 2026-06-17 · **Status:** Draft
**Repo:** DynaCharting · **Linear:** DynaCharting — Interaction Layer (project)

> **Design source:** the deferred Tier-4 subsystem scoped in
> [`../2026-06-11-universal-chart-engine/RESEARCH.md`](../2026-06-11-universal-chart-engine/RESEARCH.md)
> §3, §5.2, §6.1, §7.3, §7.4, §9.4 and that project's SPEC non-goals. This document is **both** the
> project-level SPEC **and** the "interaction layer design appendix" deliverable the research deferred
> (the ENC-621 acceptance criterion: *"a reviewed appendix doc; the two foundation hooks are confirmed
> sufficient to snap the layer on later"*).

---

## Problem

The universal-chart-engine program (ENC-589…620, all on `main`) delivered a GPU-native, streaming,
AI-authored **grammar of graphics**: a manifest is a pure one-directional **`feed → frames`** function.
That purity is exactly why interaction was carved out as a separate program — interaction is
**bidirectional**: runtime pointer/keyboard events mutate cross-frame state that must feed *back* into
the transform inputs. No declarative transform, compute kernel, or manifest node can express it.

The research deferred interaction **until the render-capability phases carried two foundation hooks**,
so the layer could "snap on later without a retrofit." Those hooks are now shipped:

- **`core/include/dc/data/RowIdentity.hpp`** (ENC-594) — every table row gets a durable id that
  survives appends and retention/eviction; the prerequisite for per-instance picking and object constancy.
- **`core/include/dc/data/ReactiveGraph.hpp`** (ENC-595) — a generic dirty/recompute engine whose
  input identity is a `(kind, key)` pair with an explicit `InputKind::Signal` and a `markSignalDirty()`
  entry point — *built* for interaction signals, not just data appends.

The gate is satisfied. This project builds the interaction layer on top of those hooks.

## Verified starting state (what already exists)

A pre-flight audit of `core/` established that **most of the expensive foundation is already in place**;
the bulk of this program is *wiring*, not greenfield:

- **Signals are pre-wired into the reactive engine.** `ReactiveGraph` exposes `InputKind::Signal` and
  `markSignalDirty(signalKey)` — but `markSignalDirty` is **never called** anywhere. The mechanism
  exists; the plumbing that drives it does not.
- **Per-instance pick is ~70% done.** `RowIdentity` mints durable ids; the **encode pass already
  threads `EncodeResult::instanceRowIds`** out-of-band for every mark; and the phase-2 instanced formats
  (`Rect4Color` 24B, `Point4Color` 16B) **reserved a row-id lane** — *no new vertex format is required*.
  Missing: GPU plumbing (upload the array, have the pick shader emit the row id, return it in the result).
- **An interaction subsystem already exists but is unwired in production.** `HoverManager`,
  `SelectionState`, `EventBus`, `DragDropState`, `FocusManager`, `InteractionCoordinator`, plus a
  `BindingEvaluator` with an `onHoverChanged` stub. None are instantiated in `JsonHost`/the WASM host.
  `HoverManager`/`SelectionState`/`EventBus` are directly reusable; `HandleSet`/`SnapManager`/`ContextMenu`
  are drawing-annotation editing (D66/D67/D50 era) and are **out of scope**.
- **Animation primitives exist but are unwired and not data-bound.** `AnimationManager`, `Tween`,
  `Easing`, `DrawItemAnimator`, `ViewportAnimator` are functional, but **no per-frame clock exists**
  anywhere (the only render loop, `live_server`, is event-driven) and nothing is keyed to row identity.

## Proposed change (the design appendix)

Add a **second, orthogonal dataflow** alongside the `feed → frames` manifest path:

```
  pointer / keyboard events
        │  (host: WASM EngineHost / windowed host)
        ▼
  ┌───────────────┐   reuse HoverManager / SelectionState / EventBus +
  │ EVENT MAPPING │   a new brush/interval gesture recognizer
  └──────┬────────┘
         ▼
  ┌───────────────┐   typed mutable runtime state:
  │ SIGNAL STORE  │   point | interval | multi selection · hover rowId · brush rect · camera ·
  │               │   transition clock.  Each signal exposes a predicate P(row)→bool.
  └──────┬────────┘
         │  markSignalDirty(signalKey)                ┌──────────────────────────┐
         ▼                                            │  PER-INSTANCE PICK PATH   │
  ┌───────────────┐  feedback edge (Signal input)     │  pixel → instance rowId   │
  │ ReactiveGraph │◀──────────────────────────────────┤  (reserved lane + shader) │
  │  (ENC-595)    │                                    └──────────────────────────┘
  └──────┬────────┘                                            ▲ routes hit → signal
         ▼  drain() → dirty transform/encode nodes
  ┌───────────────────────────────────────────────────────────┐
  │ TRANSFORM DAG / ENCODE   selection-filter transform reads   │
  │                          a signal predicate; conditional    │
  │                          encoding re-resolves color/opacity │
  └──────┬────────────────────────────────────────────────────┘
         ▼
  ┌───────────────────────────────────────────────────────────┐
  │ DATA-BOUND TRANSITIONS  frame clock + AnimationController:  │
  │   RowIdentity old→new diff → enter/exit/update tweens on    │
  │   per-instance attribute lanes (object constancy)           │
  └───────────────────────────────────────────────────────────┘
```

The renderer and the manifest grammar stay unchanged; interaction is **additive** and references the
manifest path through the *same* `ReactiveGraph` the data path already uses.

### Component design

1. **Signal store.** A typed container of named mutable signals: `pointSelection(rowId)`,
   `intervalSelection(field, [lo,hi])`, `multiSelection(disjunction of intervals)`, `hover(rowId)`,
   `brush(rect)`, `camera(pan,zoom)`, `transitionClock(t∈[0,1] per instance)`. Each signal derives a
   **predicate** `P(row) → bool`. A signal mutation calls `ReactiveGraph::markSignalDirty(signalKey)`.

2. **Predicate model.** A selection predicate compiles to a boolean column over the live table. A
   **selection-filter transform** consumes the predicate (drop or de-emphasize non-matching rows);
   **conditional encoding** (`color: {condition:{test:"isSelected", value:…}, value:…}`) re-resolves a
   channel against the predicate. Both register as `ReactiveGraph` dependents of the signal input, so
   they recompute on `drain()` exactly like data-driven nodes.

3. **Per-instance pick path.** Extend `PickResult` with a `rowId`; in `renderPick`, upload the encode
   pass's `instanceRowIds` (already collected) into the reserved instance lane / a parallel pick target;
   have the pick shaders emit the row id; decode `(drawItemId, rowId)` on readback and route it into the
   signal store. **No vertex-format change** — the lane is reserved.

4. **Event → signal wiring.** Reuse `HoverManager` (fire `markSignalDirty(hoverSignal)` on enter/exit),
   `SelectionState` (mark a selection signal on toggle), and `EventBus`. Add a **brush/interval gesture
   recognizer** (separate from the click-oriented `InteractionCoordinator`). Surface browser
   pointer/keyboard events through the **WASM `EngineHost`** into these handlers.

5. **Data-bound transitions (object constancy).** Add a **per-frame transition clock** as a lifecycle
   hook in the EngineHost/render loop. An **`AnimationController`** observes `ReactiveGraph::drain()`,
   diffs the `RowIdentity` column old→new to classify rows as **enter / exit / update**, and spawns
   tweens via `AnimationManager`. Per-instance animation targets are the encoded attribute lanes keyed
   by row id (not one DrawItem per row).

6. **Cross-view linking.** A **shared signal store** above multiple manifest instances (a
   session/application layer): a brush in view A mutates a shared signal that view B's transforms read.
   The research correctly classifies this as an application-composition concern *above* the manifest;
   it lives in the host/session layer, not in any transform node.

### Confirmation: the two hooks are sufficient

Per the ENC-621 acceptance bar — `RowIdentity` + `ReactiveGraph` are confirmed sufficient to snap the
layer on without retrofitting the data→visual grammar: signals register as `InputKind::Signal` inputs on
the *existing* graph and recompute through the *existing* `drain()`; per-instance pick rides the
*already-reserved* row-id lane fed by the *already-collected* `instanceRowIds`; object constancy keys off
the *already-durable* row ids. No change to the manifest grammar, the encode-pass stride contract, or the
10 render pipelines is required by this design.

## Scope

- Signal store + predicate model + selection-filter transform + conditional-encoding read of predicates.
- Per-instance pick **render path** (the renderer change the research deferred) + hit→signal routing.
- Event→signal wiring reusing `HoverManager`/`SelectionState`/`EventBus` + a new brush gesture, surfaced
  through the WASM host.
- Data-bound transitions: frame clock, `AnimationController` (enter/exit/update via RowIdentity diff),
  per-instance animatable lanes.
- Cross-view linking via a shared session-level signal store + a linked-brush proof.
- A proof demo + validation on an instanced view.

## Non-goals

- **Drawing/annotation editing** (`HandleSet`, `SnapManager`, `ContextMenu`, control-point handles,
  magnet-snap) — a separate TradingView-style program; left untouched.
- **True-3D rasterization** — out of charter (Pos3 + mat4 + depth-stencil; separate render program).
- **New chart types / manifest grammar changes** — this layer is additive and changes neither.
- **In-engine iterative layout** (force/t-SNE) — owned elsewhere; not interaction.

## Constraints

- **Additive only:** no change to the manifest grammar, the `validateDrawItem` exact-stride contract, or
  the 10 Dawn pipelines. Per-instance pick must use the reserved lane, not a new format.
- **One reactive engine:** signals drive the *existing* `ReactiveGraph`; do not fork a second mechanism.
- **f32-only on GPU** (epoch-ms stays CPU) — unchanged from the grammar program.
- **Frame clock** must not couple to the data tick — transitions advance on a render cadence, signals
  recompute on `drain()`; both must compose without double-recompute.

## Affected systems

- **DynaCharting only.** `core/` (`dc`, `dc_gpu`): new `SignalStore`, predicate/selection-filter,
  `AnimationController`, frame-clock hook, pick-path extension; reuse of `interaction/` + `anim/` +
  `data/ReactiveGraph` + `data/RowIdentity` + `encode/EncodePass`. `packages/dc-wasm`: EngineHost event
  surface + frame tick. `apps/showcase`/a demo: the proof view.
- No `treaty`/`forum`/`embassy` wire-contract changes; no sovereignty impact (mock/showcase data).

## Alternatives considered

- **Fold interaction into the manifest** — rejected; breaks the `feed→frames` purity that makes
  AI-authoring verifiable (the whole reason it was deferred).
- **A second bespoke reactive mechanism for signals** — rejected; `ReactiveGraph` was explicitly built
  generic (`InputKind::Signal`) to avoid this.
- **One DrawItem per row for per-instance animation/pick** — rejected; does not scale (instanced marks
  are one DrawItem by design). Use reserved per-instance lanes keyed by row id.

## Risks

| Risk | Severity | Mitigation |
|---|---|---|
| Pick-pass readback / row-id decode wrong (off-by-instance) | Med | Golden round-trip tests: instanced grid → known row; verify reserved-lane packing. |
| Double-recompute when a signal change and a data tick coincide | Med | Single `drain()` per frame; signals and data both mark the same graph; coalesce. |
| No frame clock → transitions stutter / leak | Med | Add an explicit lifecycle tick; bound active tweens; cancel on exit. |
| Reused `interaction/` classes assume drawItem semantics, not row identity | Med | Wire to row-level identity via RowIdentity; rework `InteractionCoordinator` click→row path. |
| Cross-view shared state bleeds sovereignty/scope creep | Low | Keep it a host/session layer; showcase/mock data only. |

## Acceptance criteria

- Clicking a single tile in an **instanced** view (e.g. treemap/scatter — one DrawItem, many instances)
  returns the **correct source row id** (per-instance pick round-trips).
- A **brush** over an instanced view filters/de-emphasizes non-selected rows live, via a signal predicate
  read by a transform — with **zero manifest grammar change**.
- A **hover** re-encodes (highlight/tooltip) the hovered row, driven through `ReactiveGraph` signals.
- Appending/evicting rows produces **smooth enter/exit/update transitions** keyed by RowIdentity
  (object constancy), driven by a frame clock.
- A **brush in view A filters view B** through a shared session signal store (cross-view linking).
- Proof demo runs end-to-end; `ctest` green (incl. new per-instance-pick golden tests).

## Open questions

1. Pick readback: reuse the existing single pick target + a parallel row-id buffer, or add a second
   render target storing row ids? (Lean: parallel buffer first; second target if needed.)
2. Brush gesture home: extend `InteractionCoordinator`, or a standalone gesture recognizer feeding the
   signal store? (Lean: standalone; coordinator stays click-oriented.)
3. Transition clock owner: unify with `StreamingScheduler`'s frame-time, or a dedicated render-loop tick?
4. Cross-view store: in `dc-wasm`/customer-layer (TS host) or in the C++ core session layer?

## Phased ticket map

Each ticket ≤5h. Critical path: **A → B → (C ∥ D) → E → G**; **F** depends on B.

**Phase A — Design appendix**
- **ENC-621** [A1] Interaction layer design appendix = this SPEC (reviewed). *(repurposed from the GoG project)*

**Phase B — Signals foundation** (C++ core; no Dawn)
- [B1] `SignalStore`: typed mutable selection/hover/brush/camera/clock signals + per-signal predicate.
- [B2] Wire signals into `ReactiveGraph` (`markSignalDirty` + control-plane transform↔signal dependency).
- [B3] Predicate model: selection predicate → boolean column; conditional-encoding reads the predicate.
- [B4] Selection-filter transform consuming a signal predicate (filter + de-emphasis modes).

**Phase C — Per-instance pick path** (Dawn build)
- [C1] Extend `PickResult` with `rowId`; thread `EncodeResult::instanceRowIds` into `renderPick`.
- [C2] Pick pass emits per-instance row id (upload lane / parallel pick buffer + pick-shader output).
- [C3] Route decoded `(drawItemId, rowId)` hit → `SignalStore` (click/hover → selection signal).
- [C4] Per-instance pick golden / round-trip tests (instanced grid → correct row).

**Phase D — Event → signal wiring** (C++ core + WASM)
- [D1] Wire `HoverManager` → hover signal (reuse existing).
- [D2] Wire `SelectionState` + `EventBus` → `SignalStore` (reuse existing).
- [D3] Brush / interval gesture recognizer → interval selection signal.
- [D4] Surface pointer/keyboard events through the WASM `EngineHost` → signals.

**Phase E — Data-bound transitions** (Dawn build)
- [E1] Per-frame transition clock / frame-tick lifecycle hook (EngineHost + render loop).
- [E2] `AnimationController`: observe `ReactiveGraph::drain()` + RowIdentity old→new diff → enter/exit/update tweens.
- [E3] Per-instance animatable channels (tween instance attribute lanes keyed by row id).

**Phase F — Cross-view linking** (C++ core + WASM)
- [F1] Shared `SignalStore` across multiple manifest instances (session/app layer).
- [F2] Linked-brushing demo (brush in view A filters view B).

**Phase G — Proof demo + validation** (Dawn build)
- [G1] PROOF DEMO: brush-to-filter + hover-tooltip + click-drill-down with smooth transitions on an instanced view.
