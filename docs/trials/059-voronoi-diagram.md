# Trial 059: Voronoi Diagram

**Date:** 2026-03-12
**Goal:** Pixelated Voronoi diagram with 12 seed points, 625 grid cells (25×25) colored by nearest seed, and white seed dots. Tests nearest-neighbor computation for 625 cells across 12 seeds, multi-color instancedRect@1 grouping (12 DrawItems), and center-fan dot tessellation on an 800×800 square viewport.
**Outcome:** All 625 cells assigned to correct nearest seed. All 12 seed dots at exact positions. 12 distinct color regions clearly visible. Zero defects.

---

## What Was Built

An 800×800 viewport (square) with a single pane (background #0f172a):

**625 colored grid cells (12 instancedRect@1 DrawItems, rect4):**
25×25 grid of 4×4 data-unit cells. Each cell colored by its nearest seed point.

| Seed | Position | Color | # Cells |
|------|----------|-------|---------|
| 1 | (15, 80) | #3b82f6 (blue) | 59 |
| 2 | (40, 90) | #10b981 (emerald) | 51 |
| 3 | (75, 85) | #f59e0b (amber) | 77 |
| 4 | (10, 50) | #8b5cf6 (violet) | 45 |
| 5 | (35, 55) | #ec4899 (pink) | 39 |
| 6 | (65, 60) | #ef4444 (red) | 51 |
| 7 | (90, 45) | #06b6d4 (cyan) | 56 |
| 8 | (20, 20) | #84cc16 (lime) | 78 |
| 9 | (50, 25) | #f97316 (orange) | 46 |
| 10 | (80, 15) | #a855f7 (purple) | 63 |
| 11 | (45, 45) | #14b8a6 (teal) | 28 |
| 12 | (60, 30) | #e11d48 (rose) | 32 |

Total: 625 cells (25×25). Alpha 0.85 for all fills.

**12 seed point dots (1 triAA@1 DrawItem, pos2_alpha, 576 vertices = 12 circles × 48 verts):**
White, alpha 0.9. Radius 2.0 data units. 16 segments per circle, center-fan tessellation. Layer 11.

Data space: X=[0, 100], Y=[0, 100]. Transform 50: sx=0.019, sy=0.019, tx=−0.95, ty=−0.95.

Layers: Cell fills (10) → Seed dots (11).

Total: 43 unique IDs.

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

- **All 625 grid cells assigned to correct nearest seed.** For each cell, the Euclidean distance from cell center (gx×4+2, gy×4+2) to all 12 seeds was verified. Every cell is assigned to the seed with minimum distance. 625/625 correct.

- **Cell counts are plausible.** Corner/edge seeds with few competitors have larger cells: lime (seed 8 at 20,20) has 78 cells, amber (seed 3 at 75,85) has 77. Central seeds with many neighbors have smaller cells: teal (seed 11 at 45,45) has only 28, rose (seed 12 at 60,30) has 32. This matches expected Voronoi behavior.

- **All 12 seed dots at exact positions.** Every dot center vertex matches its seed (x, y) coordinate exactly. 12/12 correct.

- **12 distinct colors create clear cell boundaries.** The color transitions between adjacent grid cells make Voronoi cell boundaries immediately visible without explicit boundary lines. Each color region is contiguous and centered on its seed.

- **Pixelated approach produces clean results.** The 25×25 grid (4×4 data-unit cells) gives sufficient resolution at 800×800 pixels. Each grid cell is 32×32 pixels, creating a crisp mosaic effect that clearly communicates the Voronoi partition.

- **No cell assigned to multiple seeds.** Every (gx, gy) position appears in exactly one buffer. No duplicates, no gaps.

- **Square viewport with equal sx=sy ensures undistorted distances.** 800×800 with sx=sy=0.019 means Euclidean distances in data space map to equal pixel distances in both axes, so the Voronoi cells have correct shapes.

- **Seed dots are clearly visible.** White at alpha 0.9 on top of colored fills (alpha 0.85) creates strong contrast. The dots mark each cell's generator point.

- **All vertex formats correct.** instancedRect@1 uses rect4 ✓, triAA@1 uses pos2_alpha ✓.

- **All buffer sizes match vertex counts.** 13/13 geometries verified.

- **All 43 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Pixelated Voronoi avoids complex polygon math.** Instead of computing exact Voronoi edges and clipping to boundaries, a grid-based nearest-neighbor approach produces a visually clear Voronoi diagram. The grid resolution controls the smoothness of cell boundaries.

2. **12 DrawItems for 12 colors is necessary.** Since instancedRect@1 applies a single color to all instances, each seed's cells need their own DrawItem. The cells are grouped by seed at generation time.

3. **Cell counts validate the Voronoi partition.** Seeds near corners/edges with few competitors should have more cells. Seeds in the center with many neighbors should have fewer. This is a quick sanity check.

4. **4×4 grid cells at 800×800 viewport = 32×32 pixels per cell.** This resolution is fine for demonstrating Voronoi regions. Finer grids (50×50) would produce smoother boundaries at the cost of more data.

5. **Color transitions serve as implicit boundaries.** No explicit boundary lines are needed — the adjacent cells of different colors create sharp visual edges that define the Voronoi partition.
