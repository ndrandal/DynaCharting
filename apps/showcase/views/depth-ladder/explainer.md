---
title: Depth Ladder — order book
referenceTool: Bookmap / DOM
tier: composed
---

A live order book for AAPL — 20 price levels per side, bid (green) growing left of the mid and ask (red) growing right, each bar's length ∝ its resting size — composed from a JSON manifest over the faithful data path. The book is a *wide current-state vector*, not a growing series, so it rides the **scalar-fan / fixed-mode** technique: each of the 40 levels is its own `depth.{bid,ask}.k.size` subscription bound `{mode:"fixed", offset:k·16+8}`, writing its value via UPDATE_RANGE into a pre-sized `rect4` buffer that the geometry reads as one live snapshot, overwritten in place every tick.

| | |
|---|---|
| **DATA** | AAPL · L2 depth · 20 levels/side · ~20s replay |
| **PIPELINE** | `instancedRect@1` (one bar per level) |
| **WRITE MODE** | fixed (UPDATE_RANGE, op 2) — current-state vector |
| **COMPOSED VIA** | 40-level scalar-fan → 2 fixed buffers (`601` bid, `602` ask), each overwritten in place each tick |
| **BUFFERS** | `601` rect4 bid (20×16B) · `602` rect4 ask (20×16B) |
| **SOURCE** | Mock GMA → embassy → dataplane WS → dc-wasm |

**What's going on (the technique).** A candle series *grows* — every tick appends a record — so it uses the append/compound write modes. An order book *doesn't grow*; it is a fixed-width vector of current sizes that is rewritten every tick. The append path can't express that, and the compound path caps at 8 join slots — far short of 40. So the ladder uses embassy's third write mode, **fixed**: a scalar-fan of one subscription per level, each bound to a fixed byte offset in a pre-sized buffer. The static bar corners (baseline x0, the level's y-band) are baked once via the manifest's `uploads`; each level's *dynamic* size drives only the far x-corner (`x1` at `k·16+8`), so a single `UPDATE_RANGE` float per level per tick redraws the whole book. The bid draw item carries a mirrored (negative-sx) transform so its bars grow left of the mid — the classic Bookmap/DOM split ladder, entirely from JSON + upstream precompute.

> Note on the synthetic feed: the mock GMA derives every `depth.*.size` field from one mid-price oscillator, so at capture all 20 levels carry near-identical sizes each tick and the ladder "breathes" together over the replay rather than showing a jagged per-level profile. The *mechanism* — 40 fixed-offset UPDATE_RANGE writes into pre-sized buffers, read as a live snapshot — is exactly what a real L2 feed would drive.
