# Trial 076: Sierpinski Triangle

**Date:** 2026-03-12
**Goal:** Sierpinski triangle fractal at recursion depth 6 (729 filled triangles) in a single triSolid@1 DrawItem on a 700×700 square viewport. Tests recursive geometric subdivision, self-similar fractal structure, triangle vertex computation at 6 levels of recursion, and dense buffer data (4,374 floats).
**Outcome:** All 729 triangles match independently computed Sierpinski recursion with 0.000000000 error. All 729 centroids unique (no overlaps). Self-similar 243/243/243 partition confirmed. 6 unique IDs. Zero defects.

---

## What Was Built

A 700×700 viewport (square) with a single pane (background #0f172a):

**729 filled triangles in 1 triSolid@1 DrawItem (pos2_clip, 2 fpv):**

Initial equilateral triangle: A=(350, 550), B=(50, 30), C=(650, 30). Sides: AB≈600.3, BC=600.0, AC≈600.3 (equilateral to within 0.3 data units).

Recursion depth 6: each level subdivides into 4 sub-triangles via edge midpoints, removes the center inverted triangle, recurses into the remaining 3. Result: 3^6 = 729 filled triangles.

Color: #06b6d4 (cyan) at alpha 0.85.

Each small triangle: area = 38.086 data units² (original 156,000 / 4^6). Total filled area = 27,765 = 17.80% of original (= (3/4)^6).

Data space: [50, 650] × [30, 550]. Transform 50: sx=0.002857, sy=0.003333, tx=ty=−1.0. Clip range: X [−0.857, 0.857], Y [−0.900, 0.833].

Total: 6 unique IDs (1 pane, 1 layer, 1 transform, 1 buffer, 1 geometry, 1 drawItem).

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation.

2. **Spec deviation: triangle orientation flipped.** The spec placed the apex at (350, 50) with the base at Y=570. The agent repositioned to apex at (350, 550) and base at Y=30 so the apex appears at the visual top (since Y increases upward in clip space). This is a correct adaptation — the spec's coordinates would have rendered the triangle pointing downward.

---

## Spatial Reasoning Analysis

### Done Right

- **All 729 triangles match independent Sierpinski computation with 0.000000000 error.** Every triangle verified against a separately implemented recursive subdivision. Bit-identical results across all 4,374 coordinate values.

- **All 729 triangle centroids are unique.** No two triangles overlap or coincide. The Sierpinski subdivision produces a partition — every point in the original triangle is in exactly one filled sub-triangle or one void.

- **Self-similar 243/243/243 partition confirmed.** The three depth-1 sub-triangles (top, bottom-left, bottom-right) each contain exactly 243 = 3^5 filled triangles at depth 6. This is the defining property of Sierpinski self-similarity.

- **All triangle areas are identical (38.086).** At depth 6, every filled triangle has area = original_area / 4^6. The uniformity confirms the recursion applies the same subdivision at every level.

- **Total filled fraction matches theory.** 729 × 38.086 = 27,765 = (3/4)^6 × 156,000. The Sierpinski triangle fills exactly (3/4)^n of the original area at depth n.

- **Original triangle is equilateral.** Sides 600.3, 600.0, 600.3 — equilateral to within 0.06% error. The slight asymmetry (600 vs ~600.33) comes from the triangle height: an equilateral triangle with base 600 has height 600·√3/2 ≈ 519.6, but the agent used height 520 (550−30). Close enough for visual purposes.

- **All vertices within clip bounds.** Clip X [−0.857, 0.857], clip Y [−0.900, 0.833] — comfortably within [−1, 1]. No clipping.

- **Buffer size matches vertex count.** 4,374 floats = 2,187 vertices × 2 fpv. 2,187 vertices = 729 triangles × 3 vertices.

- **All 6 IDs unique.** Minimal ID allocation with no collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Sierpinski at depth 6 produces 729 triangles in 4,374 floats.** This is lightweight — depth 7 (2,187 triangles, 13,122 floats) and depth 8 (6,561 triangles, 39,366 floats) would be feasible.

2. **All depth-n triangles have identical area = original / 4^n.** This is because each subdivision divides area by 4, and the recursion always keeps 3 of 4 sub-triangles. The filled fraction (3/4)^n → 0 as n → ∞ (fractal has Hausdorff dimension log₂3 ≈ 1.585).

3. **Self-similarity means each depth-1 sub-triangle contains exactly 1/3 of the total filled triangles.** This partitioning property can be verified by checking which sub-triangle each centroid falls into.

4. **Y-up clip space means data-space Y orientation matters.** The agent correctly flipped the triangle so the apex appears at the visual top. When specifying coordinates for visual trials, the spec should account for the Y direction of the target coordinate system.

5. **Single DrawItem for 729 triangles is efficient.** All triangles share the same color and pipeline, so there's no need for separate DrawItems. One buffer, one geometry, one draw call.
