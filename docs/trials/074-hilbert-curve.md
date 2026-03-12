# Trial 074: Hilbert Curve

**Date:** 2026-03-12
**Goal:** Hilbert space-filling curve at order 5 (32×32 grid, 1023 line segments) with 4-color gradient bands, on a 700×700 square viewport. Tests algorithmic d2xy mapping correctness, space-filling coverage (all 1024 cells visited exactly once), adjacency invariant (Manhattan distance 1 between consecutive cells), and color-banded curve visualization.
**Outcome:** All 1023 segments match independently computed Hilbert curve with 0.000000000 error. All 1024 cells visited exactly once. All 1023 consecutive pairs are adjacent. 16 unique IDs. Zero defects.

---

## What Was Built

A 700×700 viewport (square) with a single pane (background #0f172a):

**1023 Hilbert curve segments in 4 color bands (4 lineAA@1 DrawItems, rect4):**

| DrawItem | Segments | Color | Count |
|----------|----------|-------|-------|
| 102 | 0–255 | #ef4444 (red) | 256 |
| 105 | 256–511 | #f59e0b (amber) | 256 |
| 108 | 512–767 | #10b981 (emerald) | 256 |
| 111 | 768–1022 | #3b82f6 (blue) | 255 |

lineWidth 2.0, alpha 0.9 for all bands. 32×32 grid (order 5).

Each segment connects cell center (x+0.5, y+0.5) to next cell center. Points computed via standard Hilbert d2xy algorithm.

The 4 color bands correspond to the 4 quadrants of the Hilbert curve's recursive structure, creating the characteristic pattern visible in the PNG.

Data space: [0, 32] × [0, 32]. Transform 50: sx=sy=0.059375, tx=ty=−0.95.

Total: 16 unique IDs (1 pane, 2 layers, 1 transform, 4 buffers, 4 geometries, 4 drawItems).

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation.

2. **Last color band has 255 segments (not 256).** 1024 points yield 1023 segments, split 256+256+256+255. This is correct behavior, not a defect — the last band simply has one fewer segment.

---

## Spatial Reasoning Analysis

### Done Right

- **All 1024 cells visited exactly once.** The set of (x, y) coordinates from the d2xy mapping contains exactly 1024 unique entries covering the full 32×32 grid. This is the defining property of a space-filling curve.

- **All 1023 consecutive pairs are adjacent.** Every pair of consecutive points has Manhattan distance exactly 1 (differ by 1 in either x or y, not both). This confirms the curve moves to neighboring cells without jumps.

- **All 1023 segments match independent Hilbert computation.** Every segment endpoint was verified against an independently implemented d2xy function. Maximum error: 0.000000000 — bit-identical results.

- **4 color bands create quadrant visualization.** The first 256 cells (red) fill one quadrant, the next 256 (amber) fill another, etc. This makes the recursive structure of the Hilbert curve immediately visible — each quadrant contains a rotated/reflected copy of the whole curve at one lower order.

- **Square viewport with equal sx=sy preserves grid aspect.** 700×700 ensures the 32×32 grid renders with square cells (~20×20 pixels each).

- **Self-similar structure is visually evident.** The PNG shows the characteristic Hilbert pattern: each quadrant is a scaled, rotated copy of the whole curve, and this recursion is visible at multiple levels.

- **All buffer sizes match vertex counts.** 4/4 geometries verified.

- **All 16 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Hilbert d2xy maps 1D index to 2D grid position.** At order n, indices 0 to 4^n−1 map bijectively to all cells of a 2^n × 2^n grid. The algorithm works by extracting 2-bit pairs from the index and applying rotation/reflection at each level.

2. **Space-filling verification requires two checks.** (a) All cells visited exactly once (bijection). (b) Consecutive cells are adjacent (continuity). Both are necessary — a random permutation would pass (a) but not (b).

3. **Color banding by index reveals recursive structure.** Splitting the curve into 4 equal-length bands (one per quadrant) makes the Hilbert curve's self-similarity immediately visible. Each band fills one spatial quadrant.

4. **1024 points → 1023 segments.** The off-by-one between points and segments means the last color band has one fewer segment. This is standard for any polyline.

5. **Order 5 at 700×700 gives ~20px per cell.** This provides adequate resolution for the 2px line width to be clearly visible in each cell. Order 6 (64×64, ~10px/cell) would be tighter but still feasible.
