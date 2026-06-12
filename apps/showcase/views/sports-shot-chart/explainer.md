---
title: Shot chart — made/missed
referenceTool: NBA shot chart
tier: composed
---

An **NBA-style shot chart that replays a game** — a half-court scatter of made (green) and missed (red) shots that **accumulate live** over a ~20s timeline (a few early, the cloud thickening as the game heats up), with the court lines drawn underneath. Synthesized shots cluster into realistic zones (rim, paint, mid-range, the three-point arc, corner 3s), each appearing one (or a few) at a time. The same scatter/marker machinery a streaming market view uses, in a domain with no prices anywhere.

| | |
|---|---|
| **DATA** | synthesized · 320 shots (152 made / 168 missed) + static court geometry |
| **PIPELINE** | `triGradient@1` (streaming markers) + `line2d@1` (static court) |
| **WRITE MODE** | live — markers stream + accumulate via the replay timeline (records.json) |
| **BUFFERS** | `500` court (pos2_clip, static) · `501` shots (pos2_color4, streaming) |
| **SOURCE** | synthesized at build time → records.gen.mjs spreads it over a 20s clock |

**What's going on (the technique).** This is a LIVE accumulating view. Shot `(x, y)` positions and a made/missed outcome are synthesized deterministically at build time, then `records.gen.mjs` spreads them across a ~20s game clock and packs each shot as an APPEND frame in `records.json`. The replay engine streams those frames into one growing buffer; its `GrowthSync` advances the geometry's vertexCount as bytes land (ENC-558), so the marker cloud accumulates on screen as the game progresses. The court lines — boundary, key, free-throw circle, three-point arc, rim, backboard — are tessellated into `line2d@1` segment pairs and stay STATIC (one baked upload), drawn first, underneath. Everything is authored directly in clip space (identity transform).

**Why one colored-triangle stream (not two rect clouds).** A shot chart needs BOTH colors to accumulate live, but `instancedRect@1` carries one uniform color per draw item — so two colors would need two streaming buffers, and the replay engine's `GrowthSync` advances **only one buffer per view** (a known harness limit; a second instanced stream renders but freezes at its seed count — confirmed empirically here). So each shot is instead a small **colored quad** via `triGradient@1` (`pos2_color4` = x,y,r,g,b,a per vertex, two triangles per shot) with the made/missed color baked into the vertices. One buffer, one draw item, one `GrowthSync` — and both green and red accumulate together.

**Deviation from a static `points@1` scatter.** On WebGPU `points@1` is a 1px PointList with no size control (`di.pointSize` is ignored — see `DawnPointsBackend`), so streamed single-pixel dots would be invisible at gallery scale; the colored quad is the legible, color-per-shot, stream-able equivalent.

**The cross-domain point.** A shot chart is a two-class colored scatter accumulating over a fixed background — exactly the shape the engine already streams for markets. Swap the data domain and the renderer doesn't blink.
