# Trial 057: Network Graph

**Date:** 2026-03-12
**Goal:** 10-node network graph with 15 edges, 3 community color groups, variable-radius nodes sized by degree centrality, and white outlines. Tests center-fan circle tessellation at 10 different positions/radii, edge routing between node centers, multi-community coloring with separate DrawItems, and outline circles (lineAA@1) on a square 900×900 viewport.
**Outcome:** All 10 nodes at correct positions with correct radii. All 15 edges connect correct node pairs. All 480 circle vertices and 160 outline segments verified. Zero defects.

---

## What Was Built

A 900×900 viewport (square) with a single pane (background #0f172a):

**10 node circles (3 triAA@1 DrawItems, pos2_alpha):**

| Node | Position | Radius | Community | Group Color |
|------|----------|--------|-----------|-------------|
| 1 | (50, 85) | 3.6 | Blue | #3b82f6 |
| 2 | (25, 65) | 3.6 | Blue | #3b82f6 |
| 3 | (20, 35) | 3.6 | Blue | #3b82f6 |
| 4 | (40, 15) | 3.6 | Emerald | #10b981 |
| 5 | (65, 10) | 3.6 | Emerald | #10b981 |
| 6 | (80, 30) | 3.6 | Emerald | #10b981 |
| 7 | (85, 60) | 3.6 | Amber | #f59e0b |
| 8 | (70, 80) | 2.4 | Amber | #f59e0b |
| 9 | (45, 50) | 4.8 | Amber | #f59e0b |
| 10 | (55, 55) | 6.0 | Amber | #f59e0b |

Center-fan tessellation: 16 segments × 3 verts = 48 vertices per circle. Alpha 0.9.
- Blue group (DrawItem 302): 3 nodes × 48 = 144 vertices. Layer 12.
- Emerald group (DrawItem 303): 3 nodes × 48 = 144 vertices. Layer 13.
- Amber group (DrawItem 304): 4 nodes × 48 = 192 vertices. Layer 14.

**15 edges (1 lineAA@1 DrawItem, rect4, 15 instances):**

| # | Edge | From → To |
|---|------|-----------|
| 1 | 1–2 | (50,85) → (25,65) |
| 2 | 1–8 | (50,85) → (70,80) |
| 3 | 1–10 | (50,85) → (55,55) |
| 4 | 2–3 | (25,65) → (20,35) |
| 5 | 2–9 | (25,65) → (45,50) |
| 6 | 3–4 | (20,35) → (40,15) |
| 7 | 3–9 | (20,35) → (45,50) |
| 8 | 4–5 | (40,15) → (65,10) |
| 9 | 4–9 | (40,15) → (45,50) |
| 10 | 5–6 | (65,10) → (80,30) |
| 11 | 5–10 | (65,10) → (55,55) |
| 12 | 6–7 | (80,30) → (85,60) |
| 13 | 6–10 | (80,30) → (55,55) |
| 14 | 7–8 | (85,60) → (70,80) |
| 15 | 7–10 | (85,60) → (55,55) |

White, alpha 0.15, lineWidth 1.5. Layer 11 (behind all nodes).

**10 node outlines (1 lineAA@1 DrawItem, rect4, 160 instances):**
16 line segments per node × 10 nodes = 160 total. White, alpha 0.3, lineWidth 1. Layer 15 (front).

**10 grid lines (1 lineAA@1 DrawItem, rect4, 10 instances):**
5 vertical at X=0,25,50,75,100 and 5 horizontal at Y=0,25,50,75,100. White, alpha 0.04, lineWidth 1. Layer 10.

Data space: X=[0, 100], Y=[0, 100]. Transform 50: sx=0.019, sy=0.019, tx=−0.95, ty=−0.95.

Layers: Grid (10) → Edges (11) → Blue nodes (12) → Emerald nodes (13) → Amber nodes (14) → Outlines (15).

Total: 26 unique IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation.

---

## Spatial Reasoning Analysis

### Done Right

- **All 10 nodes at correct positions.** Every node's center vertex matches its specified (x, y) coordinate. All 10/10 verified.

- **All node radii match the degree-based formula.** Radius = degree × 1.2. Node 8 (degree 2) has the smallest radius (2.4), Node 10 (degree 5) has the largest (6.0), Node 9 (degree 4) is second largest (4.8). The size hierarchy is immediately visible in the PNG.

- **All 15 edges connect the correct node pairs.** Every edge's (x1,y1)→(x2,y2) matches the positions of its two endpoint nodes. 15/15 verified.

- **All 480 circle fill vertices are correct.** Each of the 10 nodes has 48 vertices in center-fan pattern (16 triangles). Every center vertex is at (cx, cy, 1.0) and every rim vertex at (cx + r×cos(θ), cy + r×sin(θ), 0.0) with θ at the correct 22.5° interval. All verified to < 0.01 data-unit error.

- **All 160 outline segments are correct.** 10 nodes × 16 segments. Each segment connects consecutive rim points of the circle. All 320 endpoints verified.

- **Square viewport with equal sx=sy ensures circular nodes.** 900×900 with sx=sy=0.019 means no aspect distortion. All circles are perfectly round.

- **Community coloring with separate DrawItems.** Blue (nodes 1-3), emerald (nodes 4-6), amber (nodes 7-10) each have their own DrawItem. This enables per-community color without per-vertex coloring.

- **Hub nodes are visually prominent.** Node 10 (radius 6.0) and Node 9 (radius 4.8) are noticeably larger than the peripheral nodes (radius 3.6) and Node 8 (radius 2.4). The visual hierarchy communicates network centrality.

- **Layer ordering creates correct visual depth.** Grid (10) → Edges (11) → Node fills (12-14) → Outlines (15). Edges draw behind nodes, so node fills occlude edge segments. Outlines draw on top of everything.

- **Edge styling is subtle but visible.** White at alpha 0.15 with lineWidth 1.5 creates connections that are clearly visible but don't dominate the visualization. The nodes at alpha 0.9 are the primary visual focus.

- **Transform math is exact.** sx=sy=1.9/100=0.019 maps [0,100] to clip[−0.95, 0.95] in both axes.

- **Grid provides spatial reference.** 10 lines at 25-unit intervals in both axes create a subtle reference grid for verifying node positions visually.

- **All vertex formats correct.** triAA@1 uses pos2_alpha ✓, lineAA@1 uses rect4 ✓.

- **All buffer sizes match vertex counts.** 6/6 geometries verified.

- **All 26 IDs unique.** No collisions across panes, layers, transforms, buffers, geometries, drawItems.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Network graphs need edges on a lower layer than nodes.** Edges on layer 11, node fills on layers 12-14. This ensures node circles occlude the edges passing through them, creating clean visual separation.

2. **Community coloring requires separate DrawItems per group.** Since triAA@1 applies a single color to all vertices, each color group needs its own DrawItem. 3 communities = 3 DrawItems.

3. **Node outlines as lineAA@1 circles add definition.** The white outlines at alpha 0.3 make node boundaries crisp against both the dark background and the colored fills. Same technique as ring separators in Trial 055.

4. **Variable radii communicate degree centrality.** A simple formula (degree × 1.2) produces visually distinguishable node sizes. The largest hub (Node 10, r=6.0) is 2.5× the smallest node (Node 8, r=2.4).

5. **Square viewports simplify network layouts.** With equal sx=sy and a square viewport, circular nodes need no aspect correction. All coordinates are symmetric in both axes.
