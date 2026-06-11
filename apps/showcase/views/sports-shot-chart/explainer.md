---
title: Shot chart — made/missed
referenceTool: NBA shot chart
tier: composed
---

An **NBA-style shot chart** — a half-court scatter of made (green) and missed (red) shots, with the court lines drawn underneath. Synthesized shots cluster into realistic zones (rim, paint, mid-range, the three-point arc, corner 3s), and the whole thing is baked once: two instanced-marker clouds over a `line2d@1` court. The same scatter/marker machinery a market scatter uses, in a domain with no prices anywhere.

| | |
|---|---|
| **DATA** | synthesized · 320 shots (152 made / 168 missed) + court geometry |
| **PIPELINE** | `instancedRect@1` (markers, 2 draw items) + `line2d@1` (court) |
| **WRITE MODE** | static — baked into manifest `uploads` (no live replay) |
| **BUFFERS** | `500` court (pos2_clip) · `501` made (rect4) · `502` missed (rect4) |
| **SOURCE** | synthesized at build time (no capture/embassy) |

**What's going on (the technique).** This is a STATIC computed view. Shot `(x, y)` positions and a made/missed outcome are synthesized deterministically at manifest-build time and split into two scatter clouds, colored per outcome (green made / red miss). The court lines — boundary, key, free-throw circle, three-point arc, rim, backboard — are tessellated into `line2d@1` segment pairs and drawn first, underneath. Everything is authored directly in clip space, so no transform is attached.

**Deviation from the brief — markers.** The task suggested `points@1` (pos2_clip) for the shots, but on WebGPU `points@1` is a **1px PointList with no size control** (`di.pointSize` is ignored — see `DawnPointsBackend`), which would be invisible at gallery scale. So each shot is drawn as a small **square** via `instancedRect@1` (one draw item per outcome, so each gets its own uniform color) — the same scatter idea, just legible. The court uses `line2d@1` static uploads as suggested.

**The cross-domain point.** A shot chart is a two-class colored scatter over a fixed background — exactly the shape the engine already draws for markets. Swap the data domain and the renderer doesn't blink.
