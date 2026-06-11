---
title: Volume Profile — volume by price
referenceTool: Sierra Chart / ToS TPO
tier: composed
---

A volume-by-price profile for AAPL — a vertical column of 24 price buckets, each a horizontal bar whose length ∝ the volume traded at that price — composed from a JSON manifest over the faithful data path. The profile is a *current-state vector* (a fixed set of buckets rewritten as volume accrues), so it rides the **scalar-fan / fixed-mode** technique: each bucket is its own `profile.bucket.k` subscription bound `{mode:"fixed", offset:k·16+8}`, writing via UPDATE_RANGE into a pre-sized `rect4` buffer the geometry reads as one live snapshot.

| | |
|---|---|
| **DATA** | AAPL · volume-by-price · 24 buckets · ~20s replay |
| **PIPELINE** | `instancedRect@1` (one bar per bucket) |
| **WRITE MODE** | fixed (UPDATE_RANGE, op 2) — current-state vector |
| **COMPOSED VIA** | 24-bucket scalar-fan → one fixed buffer (`601`), overwritten in place each tick |
| **BUFFERS** | `601` rect4 (24×16B) |
| **SOURCE** | Mock GMA → embassy → dataplane WS → dc-wasm |

**What's going on (the technique).** Like the order book, a volume profile is a fixed-width vector of current values, not a growing series — so it uses embassy's third write mode, **fixed**: a scalar-fan of one subscription per bucket, each bound to a fixed byte offset in a pre-sized buffer, written via `UPDATE_RANGE` each tick. The geometry reads the whole buffer as the live profile snapshot. The static bar corners are baked once via the manifest's `uploads`; each bucket's *dynamic* value drives only its right x-corner (`x1` at `k·16+8`).

> Note on the synthetic feed: the mock GMA derives every `profile.bucket.k` field from one mid-price oscillator, so the *live* value each bar receives is near-identical across buckets. To give the column the characteristic bell-shaped profile envelope (most volume near the mid price, tapering at the extremes), the manifest bakes a per-bucket *baseline* (x0) — center buckets get a long bar, edge buckets a short one — so bar length = liveValue − bakedBaseline. The live fixed-mode value still drives every bar each tick (the bars breathe with it); the baked envelope supplies the price-bucket shape a real per-bucket volume feed would carry. The *mechanism* — N fixed-offset UPDATE_RANGE writes into a pre-sized buffer, read as a live snapshot — is exactly faithful.
