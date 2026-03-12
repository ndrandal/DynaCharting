# Trial 063: Sierpinski Triangle

**Date:** 2026-03-12
**Goal:** Sierpinski triangle fractal at recursion depth 6 with 729 filled triangles (3^6), colored by 3 main sub-trees (blue, emerald, amber). Tests recursive geometric subdivision, self-similar fractal structure, equal-area triangle verification, and triSolid@1 at high triangle count on a 700×700 square viewport.
**Outcome:** All 729 triangles match expected vertex positions exactly (243/243 per group). All triangles have identical area (0.856300). Group centroids confirm correct spatial placement. Zero defects.

---

## What Was Built

A 700×700 viewport (square) with a single pane (background #0f172a):

**729 Sierpinski triangles (3 triSolid@1 DrawItems, pos2_clip):**

| Group | Sub-tree | Color | Triangles | Vertices | Centroid |
|-------|----------|-------|-----------|----------|----------|
| 0 | Bottom-left | #3b82f6 (blue) | 243 | 729 | (27.5, 18.0) |
| 1 | Bottom-right | #10b981 (emerald) | 243 | 729 | (72.5, 18.0) |
| 2 | Top | #f59e0b (amber) | 243 | 729 | (50.0, 57.0) |

Base equilateral triangle: A=(5, 5), B=(95, 5), C=(50, 82.94). Side length 90.

Each triangle: 3 vertices × 2 floats = 6 floats. 243 triangles × 6 = 1458 floats per buffer.

**Background (1 instancedRect@1 DrawItem, rect4, 1 instance):**
Dark rect on layer 10.

Data space: X=[0, 100], Y=[0, 100]. Transform 50: sx=sy=0.019, tx=ty=−0.95.

Layers: Background (10) → Blue (11) → Emerald (12) → Amber (13).

Total: 18 unique IDs.

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

- **All 729 triangles match expected vertex positions exactly.** Each group verified against independently computed Sierpinski subdivision: 243/243 matches for all 3 groups using canonical vertex comparison.

- **All triangles have identical area.** Every triangle measures 0.856300 data-unit² — exactly big_area / 4^6 = 3507.4 / 4096 ≈ 0.856300. This confirms correct recursive bisection at every level.

- **Self-similarity is visually evident.** The PNG shows the characteristic Sierpinski pattern: each of the 3 main colored regions is itself a smaller Sierpinski triangle, and this repeats at 6 levels of zoom.

- **3-color grouping correctly identifies sub-trees.** Bottom-left (blue) centroid at (27.5, 18.0), bottom-right (emerald) at (72.5, 18.0), top (amber) at (50.0, 57.0). These centroids are at 1/3 the height and properly distributed horizontally.

- **Recursive midpoint subdivision is exact.** At each level, midpoints are computed as ((a+b)/2, (a+b)/2). After 6 levels of halving, the smallest triangle side is 90/2^6 ≈ 1.406 data units, still well above floating-point precision limits.

- **Total Sierpinski area is correct.** 729 × 0.856300 = 624.24 = 3507.4 × (3/4)^6. The fractal's area ratio (3/4)^6 ≈ 0.178 means the Sierpinski triangle fills about 17.8% of the bounding equilateral triangle.

- **Square viewport with equal sx=sy preserves equilateral geometry.** 700×700 ensures the equilateral triangle appears equilateral, not distorted.

- **All vertex formats correct.** triSolid@1 uses pos2_clip ✓, instancedRect@1 uses rect4 ✓.

- **All buffer sizes match vertex counts.** 4/4 geometries verified.

- **All 18 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Sierpinski recursion: 3^n triangles, each with area = big_area / 4^n.** At depth n, the subdivision creates 3^n leaf triangles. Each has side length s/2^n and area s²√3/(4^(n+1)). The total fractal area is big_area × (3/4)^n.

2. **Grouping by depth-1 sub-tree creates natural 3-color decomposition.** The first split of the Sierpinski triangle creates 3 sub-problems. All 3^(n-1) leaves within each sub-tree share a color, making the fractal structure immediately visible.

3. **Canonical vertex comparison handles floating-point order.** Sorting each triangle's vertices by (x, y) and rounding to 3 decimals creates a unique key for comparison, regardless of vertex winding order.

4. **Equal area is the key structural invariant.** If all 729 triangles have identical area, the recursive subdivision was performed correctly at every level. This is a stronger check than just verifying vertex positions.

5. **Fractal triangle count grows as 3^n but vertex data grows as 3^n × 6 floats.** At depth 6: 729 triangles × 6 floats = 4374 floats per group, 13122 total. This is manageable but would grow rapidly at higher depths.
