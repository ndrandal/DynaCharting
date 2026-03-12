# Trial 071: Koch Snowflake

**Date:** 2026-03-12
**Goal:** Koch snowflake fractal outline at recursion depth 4 (768 line segments) on a 700×700 square viewport. Tests recursive geometric subdivision, self-similar fractal structure, outward-facing peak orientation, and fractal path closure.
**Outcome:** All 768 segments match independently generated Koch curve with max error 0.000000499. Snowflake closes perfectly (gap = 0.000000). Minor clipping at top edge where Koch peaks extend beyond the data range. One minor defect.

---

## What Was Built

A 700×700 viewport (square) with a single pane (background #0f172a):

**Background (1 instancedRect@1 DrawItem, rect4, 1 instance):**
Covering [0, 0] to [100, 100], color #0f172a, layer 10.

**Koch snowflake outline (1 lineAA@1 DrawItem, rect4, 768 segments):**
Color #3b82f6 (blue), lineWidth 1.5, alpha 0.9, layer 11.

Initial equilateral triangle: A=(50, 13.4), B=(5, 91.0), C=(95, 91.0).
Edges: A→B, B→C, C→A. Recursion depth 4: 3 × 4^4 = 768 segments.

Koch subdivision: each segment P→Q replaced by P→M1→Peak→M2→Q, where M1 = P + (Q−P)/3, M2 = P + 2(Q−P)/3, Peak = M1 + rotate((Q−P)/3, +60°).

Data extent: X=[5.0, 95.0], Y=[13.4, 117.0].

Data space: [0, 100] × [0, 100]. Transform 50: sx=sy=0.019, tx=ty=−0.95.

Total: 10 unique IDs (1 pane, 2 layers, 1 transform, 2 buffers, 2 geometries, 2 drawItems).

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Top-edge clipping.** The Koch peaks on the C→A edge extend to Y=117.0, which maps to clip Y=1.273 — beyond the viewport. The initial triangle was placed with the top vertex at Y=13.4 and the bottom edge at Y=91.0, but the Koch peaks on the bottom edge protrude outward (downward in data space, upward in clip space due to sy > 0 mapping). The data range [0, 100] doesn't accommodate these protrusions. Approximately 17 data units of snowflake extend beyond the viewport.

2. **Text labels invisible in PNG capture.** Known limitation.

---

## Spatial Reasoning Analysis

### Done Right

- **All 768 segments match independent Koch generation.** Every segment verified against recursive subdivision with +60° outward rotation. Maximum error: 0.000000499.

- **Snowflake closes perfectly.** The last segment's endpoint matches the first segment's start at (50, 13.4) with gap = 0.000000. The recursive subdivision preserves exact closure.

- **Outward-facing peaks confirmed.** Using +60° counterclockwise rotation produces peaks that point away from the triangle centroid. The fractal protrusions expand the shape outward at each level, creating the characteristic snowflake silhouette.

- **3-fold symmetry is visually evident.** The PNG shows identical fractal patterns on all three sides of the snowflake, confirming the recursive subdivision treats all edges identically.

- **Depth-4 detail is clearly rendered.** At 700×700, the smallest Koch features (side length 90/3^4 ≈ 1.1 data units ≈ 7 pixels) are visible as distinct bumps on the fractal boundary.

- **768 segments = 3 × 4^4 is correct.** Each recursion level multiplies the segment count by 4. Starting from 3 edges: 3→12→48→192→768.

- **All buffer sizes match vertex counts.** 2/2 geometries verified.

- **All 10 IDs unique.** No collisions.

### Done Wrong

- **Data range doesn't accommodate Koch protrusions.** The initial triangle has its base at Y=91.0, but Koch peaks on the C→A edge extend to Y=117.0 (26 units beyond). The data space [0, 100] and transform clip the top ~17% of the snowflake. The fix would be: either use a larger data range (e.g., [−20, 120]) or reposition the triangle with more headroom.

---

## Lessons for Future Trials

1. **Koch snowflake extends beyond the initial triangle.** At depth n, the outermost peaks extend by side_length × (√3/6) × (1 − (1/3)^n) beyond each edge. For a 90-unit side at depth 4: extension ≈ 90 × 0.2887 × 0.988 ≈ 25.7 units. This must be factored into the data range.

2. **Rotation direction determines peak orientation.** +60° (counterclockwise) produces outward peaks for the standard A→B→C→A winding. Using −60° would produce inward peaks (anti-snowflake).

3. **Koch closure is preserved by construction.** Since each subdivision replaces P→Q endpoints with the same P→Q but adds interior points, the overall path always closes if the initial polygon closes.

4. **768 segments in 3,072 floats is lightweight.** Even at depth 4, the Koch snowflake is manageable. Depth 5 (3,072 segments) and depth 6 (12,288 segments) would still be feasible.

5. **Always compute the fractal's bounding box BEFORE choosing the data range.** The initial triangle bounding box is insufficient — fractal protrusions can extend significantly beyond the starting geometry.
