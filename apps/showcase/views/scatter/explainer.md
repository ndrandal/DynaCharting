---
title: Scatter — Price × Volume
referenceTool: data-viz / sports analytics
tier: native
---

Each dot is one AAPL tick plotted as (lastPrice, cumulative volume) — two independent live streams joined into a single GPU point. The join happens in the data path, not in chart code: a dual-dynamic-slot compound buffer packs `lastPrice` into the point's x lane and `volume` into its y lane, emitting one `pos2_clip` record only when both have fired for the tick. The engine draws the growing cloud as instanced GPU points straight from the JSON manifest.

| | |
|---|---|
| **DATA** | AAPL · lastPrice × volume · ~20s replay |
| **PIPELINE** | `points@1` |
| **WRITE MODE** | compound (dual-dynamic-slot join, stride 8) |
| **BUFFERS** | `700` pos2_clip (8B: x=lastPrice, y=volume) |
| **SOURCE** | Mock GMA → embassy → dataplane WS → dc-wasm |
