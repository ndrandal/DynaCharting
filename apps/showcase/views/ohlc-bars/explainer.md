---
title: OHLC Bars — AAPL
referenceTool: Bloomberg / most platforms
tier: native
---

Per-second OHLC bars for AAPL — the open/high/low/close are packed per bucket into a single `candle6` record over the faithful data path (Mock GMA → embassy → dataplane WS) and drawn as thin instanced GPU bars straight from the JSON manifest. Same compound OHLC join as the candlestick view; only the rendered bar width differs, giving the classic Bloomberg-style OHLC stick.

| | |
|---|---|
| **DATA** | AAPL · 1s OHLC · ~20s replay |
| **PIPELINE** | `instancedCandle@1` |
| **WRITE MODE** | compound (OHLC packed into candle6, slotBits 0–3) |
| **BUFFERS** | `10100` candle6 (24B: x, o, h, l, c, halfWidth=0.08) |
| **SOURCE** | Mock GMA → embassy → dataplane WS → dc-wasm |
