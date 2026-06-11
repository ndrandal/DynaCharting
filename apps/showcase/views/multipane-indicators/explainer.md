---
title: Multi-pane — Price · RSI · MACD
referenceTool: ThinkorSwim / TradingView
tier: native
---

Three stacked panes from a single JSON manifest: AAPL price candles up top, an RSI line in the middle, and a MACD pane below (the macd and signal lines over a histogram of bars). Each pane is its own clip band with its own data→clip transform, so eight live streams land in one chart with no bespoke layout code. Candles ride the compound `candle6` join; each line is the stride-8 (recordIndex, value) trick drawn by `line2d@1`; the histogram is a `rect4` compound buffer drawn by `instancedRect@1`.

| | |
|---|---|
| **DATA** | AAPL · ohlc + rsi + macd + signal + histogram · ~20s replay |
| **PIPELINE** | `instancedCandle@1` + `line2d@1` + `instancedRect@1` |
| **WRITE MODE** | compound (candle6, stride-8 lines, rect4 bars) |
| **BUFFERS** | `700` candle6 · `710/720/730` pos2_clip · `740` rect4 |
| **SOURCE** | Mock GMA → embassy → dataplane WS → dc-wasm |
