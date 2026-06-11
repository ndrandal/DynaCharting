---
title: Footprint — bid×ask volume
referenceTool: order-flow footprint
tier: composed
---

An order-flow footprint for AAPL — bid (green) and ask (red) volume across a price×time grid — driven by the **scalar-fan / fixed-mode** technique at its largest scale here: a 160-way fan (10 price rows × 8 time columns × 2 sides), each cell its own `footprint.{bid,ask}.r.c` subscription bound `{mode:"fixed", offset}`, writing via UPDATE_RANGE (op 2) into one of two pre-sized `rect4` buffers read as live snapshots. This is the hardest of the three views, and it maps a real edge of the manifest model.

| | |
|---|---|
| **DATA** | AAPL · footprint · 10×8 cells × 2 sides · ~20s replay |
| **PIPELINE** | `instancedRect@1` (one bar per cell, two draw items) |
| **WRITE MODE** | fixed (UPDATE_RANGE, op 2) — current-state vector |
| **COMPOSED VIA** | 160-cell scalar-fan → 2 fixed buffers (`601` bid, `602` ask), each overwritten in place each tick |
| **BUFFERS** | `601` rect4 bid (80×16B) · `602` rect4 ask (80×16B) |
| **SOURCE** | Mock GMA → embassy → dataplane WS → dc-wasm |

**What's going on (the technique).** Each of the 160 cells is one fixed-mode subscription: it writes a single float via `UPDATE_RANGE` to one rect corner of a pre-sized buffer, every tick. The geometry reads the whole buffer as the live snapshot — the same mechanic as the depth ladder, fanned out to a grid.

**THE WALL (frontier limit — this is the valuable finding).** A *true 2D footprint grid with independent per-cell magnitude* is **not** expressible under the current model, and this view documents exactly why. `instancedRect@1` draws every instance through **one** shared per-draw-item transform and **one** uniform color, and a fixed binding writes **one raw absolute value** (~410 at capture) to **one** rect corner. Under a single global linear transform, a value-driven corner always lands at `clip(sx·V + tx)` — the *same* clip position for every cell, regardless of the cell's baked baseline. So the only per-cell quantity that survives is the bar **length** (`value − bakedBaseline`, anchored at the baked baseline); the value-driven edge can be tiled along the axis *perpendicular* to its growth, but **not along its own growth axis**. That kills the 2D grid: you cannot independently position *and* size a cell from a single global transform plus a single-float write.

So this view ships the closest faithful approximation: the 10×8 grid is **flattened into 80 stacked horizontal volume bars per side** (price·time ordered, level = row·8 + col), bid growing left of the mid, ask growing right — an honest "footprint as a stacked bid/ask volume profile." Every bar is driven live by its own fixed-mode UPDATE_RANGE write; only the *2D placement* is the casualty.

The full 2D footprint (independent per-cell heat color + per-cell origin, plus the per-cell bid/ask numerals via `textSDF@1`) needs **per-instance color and per-instance offset** — a packed multi-field instance format or a custom WGSL pipeline. That is precisely the "live-GPU / custom-shader-from-JSON" wall the frontier map names: everything left of it is a JSON manifest over a pure data path; this cell-grid heat is the one shape the manifest model can't currently buy.

> Note on the synthetic feed: as with the other composed views, the mock GMA derives every `footprint.*` field from one mid-price oscillator, so the 160 cells carry near-identical live values and the stack breathes together over the replay. The *mechanism* — 160 fixed-offset UPDATE_RANGE writes into two pre-sized buffers, read as live snapshots — is exactly what a real order-flow feed would drive.
