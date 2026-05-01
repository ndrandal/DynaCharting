# BindingEvaluator (D80)

Reactive data bindings between scene elements.

> **Audit note (ENC-88):** This subsystem landed in commit `0b8b7d8`
> ("dump :("), 6,631 LOC across 22 files, with no SPEC/PLAN/decision
> record. This document is the **retroactive design record** —
> what's there now, what its public surface guarantees, and how the
> existing `d80_*` tests pin its behavior. Treated as **stable** (no
> `EXP_` prefix) because the test suite covers the major paths and
> nothing in the audit suggests partial implementation.

## What it does

`BindingEvaluator` watches `EventBus` events (selection, hover,
viewport, data updates) and applies declarative effects from a
`SceneDocument.bindings` map. Effects can:

- **Filter** — emit a per-event subset of an input buffer's records
  to a derived output buffer (e.g. "show only the records the user
  selected").
- **Range** — translate a viewport range to a pre-aggregated
  buffer (e.g. "compute summary stats for the visible window").
- **Visibility** — toggle a draw item's `visible` flag based on a
  trigger condition (e.g. "show this label only when hover is on
  draw item X").
- **Color** — swap a draw item's color when a trigger condition
  fires (selection, threshold crossing).

After each evaluation the evaluator returns the list of output
`bufferId`s it mutated; the caller (typically `JsonHost` /
`ChartSession`) forwards those to GPU sync.

## Public API

```cpp
class BindingEvaluator {
  BindingEvaluator(CommandProcessor&, IngestProcessor&,
                   DerivedBufferManager&);

  void loadBindings(const std::map<Id, DocBinding>& bindings);
  void attach(EventBus&, SelectionState&);
  void detach(EventBus&);

  // Each returns the list of output buffer ids modified by the call.
  std::vector<Id> onSelectionChanged(const SelectionState&);
  std::vector<Id> onHoverChanged(Id drawItemId, std::uint32_t recordIndex);
  std::vector<Id> onViewportChanged(const std::string& viewportName,
                                    double xMin, double xMax);
  std::vector<Id> onDataChanged(const std::vector<Id>& touchedBufferIds);

  std::size_t bindingCount() const;
};
```

Lifetime: caller-owned. Construct after the dependent processors are
ready, call `loadBindings()` once, `attach()` to subscribe to the bus.
`detach()` before destruction is required if the bus is still live.

## Pinned invariants (per `core/tests/d80_*`)

| Test | Pins |
|---|---|
| `d80_1_binding_document` (261 LOC) | DocBinding parsing — JSON shape of the document `bindings` map: `id`, `trigger {type, params}`, `effect {type, target, params}`. Round-trip and rejection cases. |
| `d80_2_binding_evaluator` (430 LOC) | All four `on*Changed` paths fire effects in the right order; selection edge-detection is correct; hover record-index `(uint32_t)-1` clears; viewport range is forwarded to derived buffers; threshold trigger fires once on edge, not continuously. |
| `d80_3_binding_gl` (250 LOC) | Full GL round-trip — bindings drive vertex-buffer updates that re-render correctly via `Renderer`. |

## Performance characteristics

- `loadBindings` is O(N) with N = bindings count; allocates per-binding
  in `bindings_` plus `subscriptions_` vectors. Done once at scene load.
- Each `on*Changed` is O(K) where K = bindings matching that trigger
  type, plus the cost of the underlying effect (filter scans the input
  buffer, range looks up a derived view, visibility/color toggle a
  flag). Allocation: a small `std::vector<Id>` per call returning
  touched buffer ids; otherwise zero.
- Threshold edge detection state lives on `LiveBinding.lastThresholdState`
  — one bool per binding, no per-event allocation.

## Known limitations

- Effects do not compose (one effect per binding).
- No back-pressure: if `onDataChanged` fires faster than `IngestProcessor`
  can apply, late events are processed serially (not coalesced).
- `applyColorEffect` resolves draw items by exact `DocBindingEffect.target`
  string at every call — no lookup cache.
- Threshold trigger only edge-fires on rising; falling-edge isn't
  separately surfaced.

## TODO follow-ons

If the binding system grows further, consider:

1. **Effect composition** — a single binding driving multiple effects
   (color *and* visibility on selection).
2. **Cached target resolution** — pre-bind the `DocBindingEffect.target`
   string to a numeric id at `loadBindings` time.
3. **Back-pressure on data change** — coalesce `touchedBufferIds`
   within a frame.
4. **Falling-edge threshold trigger** — symmetric to rising.

These are not currently filed as Linear tickets; surface them
when/if needed.
