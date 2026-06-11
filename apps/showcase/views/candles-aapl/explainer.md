---
title: Candlestick — AAPL
referenceTool: TradingView
tier: native
---

Per-second OHLC candles for AAPL — streamed tick-by-tick over the faithful data path (Mock GMA → embassy → dataplane WS) and drawn as instanced GPU geometry straight from the JSON manifest. No bespoke chart code: the manifest declares the pane, buffer, and pipeline; the engine does the rest.

| | |
|---|---|
| **DATA** | AAPL · 1s OHLC · ~20s replay |
| **PIPELINE** | `instancedCandle@1` |
| **WRITE MODE** | compound (OHLC packed into candle6) |
| **BUFFERS** | `10100` candle6 (24B: x, o, h, l, c, halfWidth) |
| **SOURCE** | Mock GMA → embassy → dataplane WS → dc-wasm |
