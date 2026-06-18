# DESIGN: Cross-view linking (Phase F)

**Project:** DynaCharting — Interaction Layer · **Tickets:** ENC-638 (F1), ENC-639 (F2)
**Status:** Design · **Depends on:** B1–B4 (signals), B5a/B5b (render bridge + `InteractionRuntime`) — all merged.

> Companion to [`SPEC.md`](./SPEC.md). Cross-view linking is the research's
> [`RESEARCH.md`](../2026-06-11-universal-chart-engine/RESEARCH.md) §7.3/§7.4 "cross-view linked
> selection = session state" — an **application/session layer above the manifest**, not a transform.

## Problem

A selection should be able to span **multiple coordinated views**: brush an interval in view A and
view B filters/highlights the matching rows (linked brushing / cross-filtering). But the unit we
built is **per-view**: one `InteractionRuntime` over one `TransformDag`, with a `SignalStore` bound
to *that* DAG's `ReactiveGraph` (`SignalStore::setGraph` takes a single `ReactiveGraph*`). Nothing
lets two views read the *same* selection, and a signal mutation only dirties *one* DAG. There is no
"these views share this signal" construct.

This is deliberately outside the `feed→frames` manifest charter (a manifest is one view's pure
function); cross-view coordination is **composition of views**, so it lives in a thin session layer.

## Architecture

Two additions, both small and additive:

```
            ┌──────────────────────────────────────────────────────┐
            │  ViewSession  (NEW, session/app layer)                 │
            │   • owns the shared SignalStore                        │
            │   • owns N Views { TransformDag, InteractionRuntime }  │
            │   • routes a gesture from any view into the shared     │
            │     signal, then refreshes every view                 │
            └───────────┬───────────────────────────┬──────────────┘
                        │ set(brush)                 │
                        ▼                             │ refresh() each view
            ┌───────────────────────────┐            │
            │  SignalStore (shared)      │            │
            │  notify() fans markSignal- │            │
            │  Dirty to ALL subscribed   │            │
            │  ReactiveGraphs (NEW)      │            │
            └─────┬──────────────┬───────┘            │
                  ▼              ▼                     ▼
        viewA.dag.reactive()  viewB.dag.reactive()  (each view's selectionFilter
        (dependents dirtied)  (dependents dirtied)   reads the SAME signal id)
```

### 1. `SignalStore`: single graph → multiple subscribers (the only core change)

Today: `ReactiveGraph* graph_` + `notify(sig){ if(graph_) graph_->markSignalDirty(sig); }`.

Change to a **subscriber set** (additive, back-compatible):
- `void addGraph(ReactiveGraph*)` / `removeGraph(ReactiveGraph*)`; keep `setGraph(g)` as sugar for
  "clear + add one" so all existing call sites (B1–B5 tests) keep working unchanged.
- `notify(sig)` fans `markSignalDirty(sig)` to **every** subscribed graph.

That single change makes one `SignalStore` drive any number of view DAGs — each view's
`selectionFilter`/conditional node (wired via `TransformDag::addSignalDependency`, ENC-624) is
scheduled when the shared signal mutates, regardless of which view originated the gesture.

### 2. `ViewSession`: the orchestrator (new, host/session layer)

```cpp
class ViewSession {
 public:
  SignalStore& signals();                       // the shared store
  // Register a view; its DAG's reactive graph is subscribed to the shared store.
  ViewId addView(TransformDag& dag, InteractionRuntime& rt);
  // A gesture from any view mutates the shared signal, then every view re-renders.
  void setSignal(Id sig, SignalValue v);        // signals_.set(...) + refreshAll()
  void refreshAll();                            // rt.refresh() for each view
};
```

`refreshAll()` is correct-but-simple (refresh every view). An optimization (skip views whose
dependents weren't scheduled) is available later via `ReactiveGraph::isPending` per view, but is
**not** needed for correctness — leave it out of F1.

## The linked-brush flow (F2 demo)

1. User brushes in **view A** → A's brush gesture (D3, owned elsewhere) computes an
   `IntervalSelection{field, lo, hi}`.
2. `session.setSignal(kBrush, interval)` → shared `SignalStore` fans `markSignalDirty(kBrush)` to
   **both** A's and B's `ReactiveGraph`.
3. `refreshAll()` → each view's `InteractionRuntime::refresh()` re-evaluates its DAG; **both**
   views' `selectionFilter` nodes read the same `kBrush` signal and drop/flag rows.
4. A and B both re-encode → linked filtering, live.

## Cross-view field semantics (the one real subtlety)

A shared selection links views **through a shared field identity**. `IntervalSelection.field` is an
`Id`; for A's brush to filter B, B's `selectionFilter` must reference the **same field id** (e.g. a
shared "time" or join-key column id). Views over genuinely different fields link by **row id**
(point/multi selection over `RowIdentity`) instead — which already works since row ids are durable
and global. Document both modes; the demo uses a shared field (time) for the interval case and row
ids for the point case.

## Ticket scope

**ENC-638 (F1)** — shared signal store + session layer:
- `SignalStore` multi-graph subscribers (`addGraph`/`removeGraph`; `setGraph` = clear+add).
- `ViewSession` (owns shared store + views; `setSignal` → fan + `refreshAll`).
- Unit test (no GPU): two `TransformDag`s, each a `selectionFilter` reading the same signal id over
  the same field; one `ViewSession.setSignal(interval)`; assert **both** views' filtered output
  (via their `InteractionRuntime`s) reflects the selection; a row-id point selection links both too.

**ENC-639 (F2)** — linked-brushing demo (needs `pnpm`/browser + visual sign-off):
- Two coordinated views sharing a `ViewSession`; brushing A live-filters B. Reuses the D3 brush
  gesture + the WASM `EngineHost` event surface (D4). Visual proof.

## Non-goals / boundary
- Not a transform, not in the manifest — `ViewSession` is host/session code.
- No cross-process / multi-tab sync (single session only).
- The render path is unchanged: each view still goes table/transform → encode → scene via B5.
```
