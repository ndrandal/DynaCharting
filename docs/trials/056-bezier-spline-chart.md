# Trial 056: Bezier Spline Chart

**Date:** 2026-03-12
**Goal:** 3 cubic Bezier curves with control point handles and dots, showing different curve shapes. Tests cubic Bezier formula B(t) = (1−t)³P0 + 3(1−t)²tP1 + 3(1−t)t²P2 + t³P3, 49-segment polyline approximation (lineAA@1), control handle visualization, and center-fan tessellated dots (triAA@1) on a 1000×600 viewport.
**Outcome:** All 3 curves match the cubic Bezier formula with 0.000000 maximum error. All 12 control points at correct positions. All 6 handles connect correct P0→P1 and P2→P3 pairs. Zero defects.

---

## What Was Built

A 1000×600 viewport with a single pane (background #0f172a):

**3 cubic Bezier curves (3 lineAA@1 DrawItems, rect4, 49 segments each):**

| Curve | P0 | P1 | P2 | P3 | Color | Shape |
|-------|----|----|----|----|-------|-------|
| 1 | (5, 10) | (25, 55) | (50, 60) | (95, 20) | #3b82f6 (blue) | Rising hump, descending |
| 2 | (5, 45) | (30, 5) | (65, 75) | (95, 40) | #10b981 (emerald) | S-curve (dip then rise) |
| 3 | (5, 70) | (40, 30) | (75, 80) | (95, 55) | #f59e0b (amber) | Descending dip, recovery |

Each curve is sampled at 50 points (t = 0, 1/49, 2/49, ..., 1) producing 49 line segments. Alpha 0.9, lineWidth 2.5.

**6 control handles (1 lineAA@1 DrawItem, rect4, 6 instances):**
| Handle | From → To | Curve |
|--------|-----------|-------|
| 1 | (5, 10) → (25, 55) | Curve 1 P0→P1 |
| 2 | (50, 60) → (95, 20) | Curve 1 P2→P3 |
| 3 | (5, 45) → (30, 5) | Curve 2 P0→P1 |
| 4 | (65, 75) → (95, 40) | Curve 2 P2→P3 |
| 5 | (5, 70) → (40, 30) | Curve 3 P0→P1 |
| 6 | (75, 80) → (95, 55) | Curve 3 P2→P3 |

White, alpha 0.2, lineWidth 1. Layer 11 (behind curves).

**12 control point dots (1 triAA@1 DrawItem, pos2_alpha, 576 vertices = 12 circles × 48 verts):**
White, alpha 0.5. Aspect-corrected radii. 16 segments per circle, center-fan tessellation. Layer 13 (front).

4 control points per curve × 3 curves = 12 dots at: (5,10), (25,55), (50,60), (95,20), (5,45), (30,5), (65,75), (95,40), (5,70), (40,30), (75,80), (95,55).

**4 grid lines (1 lineAA@1 DrawItem, rect4, 4 instances):**
At Y=20, 40, 60, 80. Spanning X=[0, 100]. White, alpha 0.06, lineWidth 1. Layer 10.

Data space: X=[0, 100], Y=[0, 90]. Transform 50: sx=0.019, sy=0.021111, tx=−0.95, ty=−0.95.

Layers: Grid (10) → Handles (11) → Curves (12) → Control Points (13).

Total: 24 unique IDs.

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

- **All 3 Bezier curves match the cubic formula exactly.** B(t) = (1−t)³P0 + 3(1−t)²tP1 + 3(1−t)t²P2 + t³P3 evaluated at 50 evenly-spaced t values. Maximum error across all 3 curves: 0.000000 (within float precision). All 147 segments verified.

- **Curve endpoints are exact.** Curve 1 starts at (5, 10) and ends at (95, 20). Curve 2: (5, 45) → (95, 40). Curve 3: (5, 70) → (95, 55). All 6 endpoints match their respective P0 and P3 values exactly.

- **All 6 control handles connect correct control points.** Each curve has two handles: P0→P1 (from start to its control point) and P2→P3 (from the end's control point to the endpoint). All 6 handle endpoints verified against the control point table.

- **All 12 control point dots at correct positions.** Each dot center matches its (x, y) from the control point specification. 12/12 exact.

- **Control point dot tessellation is correct.** 16 segments × 3 vertices = 48 vertices per dot in center-fan pattern. 12 dots × 48 = 576 total vertices. Aspect-corrected radii (rx ≈ 0.526316, ry ≈ 0.789478 data units) produce ~5px circles in both axes.

- **49-segment polyline produces smooth curves.** At 1000px viewport width, each segment spans ~18.4 pixels on average. The polyline approximation is visually indistinguishable from a true curve.

- **Three distinct curve shapes demonstrate Bezier expressiveness.** Curve 1 rises then falls (arch), Curve 2 dips then rises (S-curve), Curve 3 descends then recovers. The control point positions create meaningfully different shapes.

- **Layer ordering creates proper visual hierarchy.** Grid (10, back) → Handles (11) → Curves (12) → Dots (13, front). Handles are subtle behind the curves, dots sit on top of everything for visibility.

- **Handle styling (low alpha) avoids visual clutter.** White at alpha 0.2 makes handles visible but not dominant. The curves at alpha 0.9 with lineWidth 2.5 are the primary visual focus.

- **Transform math is exact.** sx=1.9/100=0.019 and sy=1.9/90≈0.021111 correctly map X=[0,100], Y=[0,90] to clip[−0.95, 0.95].

- **All vertex formats correct.** lineAA@1 uses rect4 ✓, triAA@1 uses pos2_alpha ✓.

- **All buffer sizes match vertex counts.** 6/6 geometries verified: buf100=16/4=4 ✓, buf101=196/4=49 ✓, buf102=196/4=49 ✓, buf103=196/4=49 ✓, buf104=24/4=6 ✓, buf105=1728/3=576 ✓.

- **All 24 IDs unique.** No collisions across panes (1), layers (10-13), transform (50), buffers (100-105), geometries (200-205), drawItems (300-305).

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Cubic Bezier at 49 segments produces pixel-perfect curves.** 50 sample points (t = k/49 for k=0..49) yield 49 line segments that are visually smooth at 1000px width. This is a good default for any parametric curve.

2. **Control handles as separate low-alpha lines add educational value.** Showing the handle lines (P0→P1, P2→P3) makes the relationship between control points and curve shape immediately visible without cluttering the visualization.

3. **Center-fan tessellated dots at control points need aspect correction.** Since sx ≠ sy, circular dots in pixel space require different radii in data space: rx = target_px / (sx × W/2), ry = target_px / (sy × H/2).

4. **Shared handle/dot DrawItems are efficient.** All 6 handles share one lineAA@1 DrawItem (same color/alpha). All 12 dots share one triAA@1 DrawItem. Only the curves need separate DrawItems due to different colors.

5. **Bezier verification requires evaluating the formula at each sample point.** The cubic Bezier formula B(t) = (1−t)³P0 + 3(1−t)²tP1 + 3(1−t)t²P2 + t³P3 can be checked against the vertex data at each t = k/49. This catches any computational errors in the control point math.
