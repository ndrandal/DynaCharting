---
title: Fund-Flow Sankey — ribbon tessellation
referenceTool: fund-flow Sankey
tier: composed
---

A **live** fund-flow Sankey — weighted ribbons connecting two columns of nodes, ribbon width ∝ flow magnitude — with **no native ribbon primitive**. The diagram is *tessellated into a colored-triangle list* and drawn by the generic `triGradient@1` pipeline (vertex format `pos2_color4`: position + per-vertex RGBA). Each ribbon is one quad (2 triangles) coloured by its source; each node is a vertical bar whose height ∝ its total flow. The flow matrix **drifts over time**: the geometry is *re-tessellated every frame* and replayed as an in-place vertex-buffer overwrite, so the ribbons pulse and re-route live.

| | |
|---|---|
| **DATA** | 4 source sectors (AAPL/MSFT/NVDA/TSLA) → 3 buckets (Inflow/Rotation/Outflow) |
| **PIPELINE** | `triGradient@1` (per-vertex colored triangles) |
| **WRITE MODE** | `updateRange` (op 2) — re-tessellated geometry overwritten in place per frame |
| **COMPOSED VIA** | flow matrix → quad tessellation (ribbons + node bars), re-run at N timesteps |
| **BUFFERS** | `601` pos2_color4 ribbons · `602` pos2_color4 node bars (pre-sized, constant vertex count) |
| **SOURCE** | `records.gen.mjs` — geometry-frame timeline (40 frames / 20s) |

**What's going on (the technique).** A Sankey has no first-class primitive in the engine — but arbitrary 2D geometry does, via `triGradient@1`'s per-vertex colored triangle list. So the whole diagram is *computed*: a flow matrix is laid out into two node columns (heights ∝ flow), with one quad per flow from the source node's right edge to the destination node's left edge, packing 6 `pos2_color4` vertices per quad with the source's tint. Node bars are emitted the same way. **To make it live**, the flow matrix is re-computed at N timesteps (each cell breathes by a smooth per-cell phase-offset oscillation), the geometry is re-tessellated each timestep, and `records.gen.mjs` emits one `UPDATE_RANGE` record per buffer per frame that overwrites the ribbon/node vertex buffers in place. The replay engine streams those frames on the recorded timeline; the `triGradient` backend re-reads the overwritten vertex buffer each frame (ENC-569), so the ribbon widths and routing pulse and re-weight live. The vertex count is held constant across frames, so the pre-sized buffers stay stable and every frame is a full-buffer overwrite at offset 0.

> **Synthesized flow & straight ribbons (two documented approximations).** (1) The flow matrix is *synthesized* from the datasets — each symbol's total traded volume split into three buckets by its net price drift over the tape — not a real capital-flow feed; it deterministically exercises the tessellation. (2) The ribbons are **straight quads**, not bezier-curved. A reference Sankey curves each ribbon along a horizontal cubic Bézier; that is purely additional tessellation in this same build step (≈16 quad segments per flow along the curve). The straight form proves the identical capability — arbitrary colored geometry from a flow matrix — at a fraction of the vertex count.
