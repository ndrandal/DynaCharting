---
title: Candles + Volume — AAPL
referenceTool: TradingView
tier: native
---

Per-second AAPL candles in the price pane with a cumulative-volume sub-pane below and a 20-period SMA overlay — three coordinated series across two panes, declared by one JSON manifest over the faithful data path (Mock GMA → embassy → dataplane WS). Candles ride a compound OHLC join (`candle6`); volume bars are a compound `rect4` (recordIndex-bracketed x, value y) on their own pane and transform, since volume dwarfs price; the SMA(20) is a thick anti-aliased polyline (`lineAA@1`) drawn over the candles on the price transform.

| | |
|---|---|
| **DATA** | AAPL · 1s OHLC + volume + SMA(20) · ~20s replay |
| **PIPELINE** | `instancedCandle@1` + `instancedRect@1` + `lineAA@1` |
| **WRITE MODE** | compound (OHLC packed) + compound (volume rect) + segment (SMA rect4) |
| **BUFFERS** | `10100` candle6 · `10130` rect4 (volume) · `10120` rect4 (SMA line segments) |
| **SOURCE** | Mock GMA → embassy → dataplane WS → dc-wasm |

All three series now grow live together: the replay engine advances every
series' vertex count as its buffer streams in (multi-buffer growth), so the
volume bars **and** the SMA line populate across the tape alongside the candles.
The SMA(20) is authored as a connected polyline — one overlapping `rect4`
segment per timestep (`[prevPoint, newPoint]`) — and drawn with `lineAA@1`, a
thick AA line whose backend re-counts its instances (`bytes ÷ 16`) on every
buffer version bump, so the streamed segments extend the drawn line live. This
replaces the earlier `line2d@1` path (a WebGPU LineList of disconnected vertex
pairs that rendered thin/broken). See ENC-587 for the line-pipeline fix lineage.
