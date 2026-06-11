---
title: Price Area — AAPL
referenceTool: TradingView baseline/area
tier: native
---

AAPL lastPrice streamed tick-by-tick over the faithful data path (Mock GMA → embassy → dataplane WS) and drawn as a filled baseline-area band straight from the JSON manifest. A single scalar field becomes an area via the compound join — embassy packs each tick into a `rect4` record whose x brackets the record index, y0 is a constant baseline, and y1 is the streamed price — so `instancedRect@1` fills each column from the baseline up to the price, abutting into a solid area-to-baseline (TradingView's area mode), with no bespoke chart code.

| | |
|---|---|
| **DATA** | AAPL · 1s lastPrice · ~20s replay |
| **PIPELINE** | `instancedRect@1` |
| **WRITE MODE** | compound (recordIndex-bracketed x, const baseline, value y → rect4, slotBit 0) |
| **BUFFERS** | `10110` rect4 (16B: x0, y0=405 baseline, x1, y1=lastPrice) |
| **SOURCE** | Mock GMA → embassy → dataplane WS → dc-wasm |
