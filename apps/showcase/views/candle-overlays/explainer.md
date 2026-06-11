---
title: Candles + Volume — AAPL
referenceTool: TradingView
tier: native
---

Per-second AAPL candles in the price pane with a cumulative-volume sub-pane below — multiple coordinated series across two panes, declared by one JSON manifest over the faithful data path (Mock GMA → embassy → dataplane WS). Candles ride a compound OHLC join (`candle6`); volume bars are a compound `rect4` (recordIndex-bracketed x, value y) on their own pane and transform, since volume dwarfs price. A 20-period SMA is also subscribed and captured so the real multi-buffer pipeline is exercised end to end.

| | |
|---|---|
| **DATA** | AAPL · 1s OHLC + volume (+ SMA(20) captured) · ~20s replay |
| **PIPELINE** | `instancedCandle@1` + `instancedRect@1` |
| **WRITE MODE** | compound (OHLC packed) + compound (volume rect) |
| **BUFFERS** | `10100` candle6 · `10130` rect4 (volume) · `10120` pos2_clip (SMA, captured) |
| **SOURCE** | Mock GMA → embassy → dataplane WS → dc-wasm |

The SMA(20) line and the live-growing volume are the frontier here: the replay
harness advances one series' vertex count per view (candles), and a connected,
width-controlled streamed line is a separate engine step — so the SMA is captured
but not yet drawn, and volume renders its seeded slice. Both data series are in
`records.json`, ready for a multi-series replay run.
