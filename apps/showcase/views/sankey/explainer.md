---
title: Fund-Flow Sankey — ribbon tessellation
referenceTool: fund-flow Sankey
tier: composed
---

A fund-flow Sankey — weighted ribbons connecting two columns of nodes, ribbon width ∝ flow magnitude — with **no native ribbon primitive**. The diagram is *tessellated at manifest-build time* into a colored-triangle list and drawn by the generic `triGradient@1` pipeline (vertex format `pos2_color4`: position + per-vertex RGBA). Each ribbon is one quad (2 triangles) coloured by its source; each node is a vertical bar whose height ∝ its total flow.

| | |
|---|---|
| **DATA** | 4 source sectors (AAPL/MSFT/NVDA/TSLA) → 3 buckets (Inflow/Rotation/Outflow) |
| **PIPELINE** | `triGradient@1` (per-vertex colored triangles) |
| **WRITE MODE** | static `uploads` — geometry baked once at manifest time |
| **COMPOSED VIA** | flow matrix → quad tessellation (ribbons + node bars) in `manifest.ts` |
| **BUFFERS** | `601` pos2_color4 ribbons · `602` pos2_color4 node bars |
| **SOURCE** | precomputed from `data/market/*` by `.build-composed-static.mjs` |

**What's going on (the technique).** A Sankey has no first-class primitive in the engine — but arbitrary 2D geometry does, via `triGradient@1`'s per-vertex colored triangle list. So the whole diagram is *computed*: the manifest reads a flow matrix, lays out two node columns (heights ∝ flow), and emits one quad per flow from the source node's right edge to the destination node's left edge, packing 6 `pos2_color4` vertices per quad with the source's tint. Node bars are emitted the same way. The geometry is uploaded once as static `uploads` (no replay, no embassy) — a pure build-time composition that the engine renders verbatim.

> **Synthesized flow & straight ribbons (two documented approximations).** (1) The flow matrix is *synthesized* from the datasets — each symbol's total traded volume split into three buckets by its net price drift over the tape — not a real capital-flow feed; it deterministically exercises the tessellation. (2) The ribbons are **straight quads**, not bezier-curved. A reference Sankey curves each ribbon along a horizontal cubic Bézier; that is purely additional tessellation in this same build step (≈16 quad segments per flow along the curve). The straight form proves the identical capability — arbitrary colored geometry from a flow matrix — at a fraction of the vertex count.
