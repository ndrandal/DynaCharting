# Trial 033: Donut Chart

**Date:** 2026-03-12
**Goal:** Six-segment donut chart (annular ring) showing expense categories. Each segment is an arc between inner radius 3.0 and outer radius 5.5, tessellated with triSolid@1 trapezoid quads. Tests annular sector tessellation, segment boundary alignment (zero gaps between adjacent arcs), and wrap-around continuity (Savings→Housing at 360°/0°).
**Outcome:** All 6 arc segments are exact. All vertices lie on the correct radii. All 5 inter-segment boundaries and the wrap-around boundary have zero gap. Arc angles sum to exactly 360°. Zero defects.

---

## What Was Built

A 700×700 viewport (square) with a single pane (clipX/Y [−0.95, 0.95], background #111827):

**6 arc segments (triSolid@1, pos2_clip):**

| Category | Percentage | Arc (°) | Quads | Vertices | Color |
|----------|-----------|---------|-------|----------|-------|
| Housing | 30% | 0–108 | 22 | 132 | #3b82f6 (blue) |
| Food | 15% | 108–162 | 11 | 66 | #10b981 (emerald) |
| Transport | 12% | 162–205.2 | 9 | 54 | #f59e0b (amber) |
| Healthcare | 10% | 205.2–241.2 | 8 | 48 | #ec4899 (pink) |
| Entertainment | 9% | 241.2–273.6 | 7 | 42 | #8b5cf6 (violet) |
| Savings | 24% | 273.6–360 | 18 | 108 | #06b6d4 (cyan) |
| **Total** | **100%** | **360°** | **75** | **450** | |

Each quad = 2 triangles (outer_i, inner_i, outer_{i+1}) + (outer_{i+1}, inner_i, inner_{i+1}).

**6 separator lines (1 lineAA@1 DrawItem, rect4, 6 instances, layer 16):**
White, alpha 0.4, lineWidth 1. Radial lines from inner to outer radius at each segment boundary.

Outer radius: 5.5, inner radius: 3.0. Data space: X/Y=[−7, 7]. Transform: sx=sy=0.135714, tx=ty=0.

Layers: Housing (10) → Food (11) → Transport (12) → Healthcare (13) → Entertainment (14) → Savings (15) → separators (16).

Text overlay: title + 6 category labels at mid-arc positions = 7 labels.

Total: 1 pane, 7 layers, 1 transform, 7 buffers, 7 geometries, 7 drawItems = 30 IDs.

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

- **All 450 vertices lie on the correct radii.** Every vertex in every segment is at distance R_out=5.5 or R_in=3.0 from the origin. Zero radius errors across all 6 segments.

- **All inter-segment boundaries have zero gap.** Housing→Food, Food→Transport, Transport→Healthcare, Healthcare→Entertainment, Entertainment→Savings: the last outer/inner vertices of each segment exactly equal the first outer/inner vertices of the next. Gap = 0.000000 for all 5 boundaries.

- **Wrap-around boundary is exact.** Savings ends at (5.5, 0) / (3.0, 0) and Housing starts at (5.5, 0) / (3.0, 0). The donut closes perfectly.

- **Arc angles sum to exactly 360°.** 108 + 54 + 43.2 + 36 + 32.4 + 86.4 = 360.0°.

- **Separator lines are exact.** All 6 radial lines connect (R_in·cos θ, R_in·sin θ) to (R_out·cos θ, R_out·sin θ) at the correct boundary angles.

- **Quad tessellation density scales with arc length.** Housing (30%, largest) gets 22 quads (~4.9° each). Entertainment (9%, smallest) gets 7 quads (~4.6° each). Consistent angular resolution across segments.

- **The donut is perfectly circular.** Square viewport (700×700), square pane, symmetric data range, sx=sy → aspect ratio 1.0. No distortion.

- **Visual proportions match data.** Housing (blue, 30%) is clearly the largest segment. Savings (cyan, 24%) is the second largest. Entertainment (violet, 9%) is the smallest. The visual proportions are immediately readable.

- **All vertex formats correct.** triSolid@1 uses pos2_clip ✓, lineAA@1 uses rect4 ✓.

- **All vertex counts match.** Housing: 264/2=132 ✓. Food: 132/2=66 ✓. Transport: 108/2=54 ✓. Healthcare: 96/2=48 ✓. Entertainment: 84/2=42 ✓. Savings: 216/2=108 ✓. Separators: 24/4=6 ✓.

- **All 30 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Annular sectors tessellate as quad strips between two concentric circles.** For each angular subdivision, 2 triangles form a trapezoid. This is the same pattern as Trial 026's band triangulation, but in polar coordinates.

2. **Segment count per arc should be proportional to arc angle.** Using ceil(arc_degrees / 5) gives ~5° per subdivision, ensuring smooth curves even for small arcs while not over-tessellating large ones.

3. **triSolid@1 is sufficient for large donut segments.** AA fringe (triAA@1) would add smoothness at the inner/outer circle edges, but for segments this large (dozens of pixels wide), the aliasing is imperceptible. Using triSolid@1 halves the vertex count.

4. **Separator lines on the top layer prevent visual merging.** Without the white radial lines, adjacent segments of similar brightness could blend at their boundary. The thin lines ensure every segment is visually distinct.

5. **Square viewports simplify circular charts.** With aspect ratio 1.0, circles remain circular without any correction. This is the natural choice for pie/donut/radar visualizations.
