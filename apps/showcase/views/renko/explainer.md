---
title: Renko — fixed-brick
referenceTool: Renko / Point & Figure
tier: composed
---

A time-less Renko chart of AAPL: price is quantised into fixed-size bricks — a green up-brick or red down-brick is drawn for every brick-sized close move, advancing one column per brick — so the x-axis tracks *movement*, not the clock. The brick wall is **tessellated at manifest-build time** from the AAPL close series (`brickSize = priceRange / 14`, walked boundary-by-boundary into axis-aligned rects) and embedded as static manifest `uploads`; there is no streaming, capture, or replay.

| | |
|---|---|
| **DATA** | AAPL · close series (40 buckets) · quantised to ~25 bricks |
| **TECHNIQUE** | build-time tessellation → static `uploads` (no replay) |
| **PIPELINE** | `instancedRect@1` (one rect4 per brick) |
| **GEOMETRY** | `rect4` (x0,y0,x1,y1) baked once via UPDATE_RANGE |
| **BUFFERS** | `601` rect4 up-bricks · `602` rect4 down-bricks |
| **COLOR** | per-draw-item — up green, down red (split across two buffers) |

**What's going on (the technique).** A streaming candle view grows its buffer tick-by-tick over the live data path. Renko is the opposite: a *static computed-geometry* view whose entire shape is resolved once, at build time, directly from a dataset. `manifest.ts` imports `AAPL.json`, derives the brick size from the close range, walks the closes emitting one fixed-height brick per boundary crossing, and emits the bricks as two pre-sized `rect4` buffers (up / down) filled by static `uploads`. `instancedRect@1` carries a single uniform color per draw item, so the two directions are drawn as two items over the same baked transform — the classic green/red Renko wall, computed entirely in deterministic TypeScript with no upstream feed.
