# Trial 028: Parallel Coordinates Plot

**Date:** 2026-03-12
**Goal:** Six-axis parallel coordinates plot showing 18 cars in 3 color-coded groups (Economy, Sport, Luxury). Each car is a polyline connecting normalized values across MPG, Cylinders, Displacement, Horsepower, Weight, and Acceleration axes. Tests lineAA@1 at high density (90 line segments), value normalization across different scales, and multi-group color coding.
**Outcome:** All 90 line segments, 6 axis lines, normalization values, and transform math are exact. Group clustering patterns are visually correct. Zero defects.

---

## What Was Built

A 1100×550 viewport with a single pane (clipX [−0.909, 0.909], clipY [−0.818, 0.818], dark background #0f172a):

**6 vertical axis lines (1 lineAA@1 DrawItem, rect4, 6 instances):**
At X=0..5, Y=0..1. White, alpha 0.15, lineWidth 1.

**3 polyline groups (3 lineAA@1 DrawItems, rect4, 30 instances each):**

| Group | DrawItem | Color | Alpha | Cars | Segments |
|-------|----------|-------|-------|------|----------|
| Economy | 105 | #34d399 (green) | 0.7 | 6 | 30 |
| Sport | 108 | #f97316 (orange) | 0.7 | 6 | 30 |
| Luxury | 111 | #a78bfa (purple) | 0.7 | 6 | 30 |

Each car produces 5 segments (6 axes − 1). 18 cars × 5 = 90 total line segments.

Normalization ranges: MPG [10,45], Cylinders [3,9], Displacement [80,400], Horsepower [70,320], Weight [1800,5000], Acceleration [6,18]. All values mapped to [0,1] on the Y axis.

Data space: X=[0, 5], Y=[0, 1]. Transform: sx=0.363636, sy=1.636364, tx=−0.909091, ty=−0.818182.

Layers: axes (10) → economy (11) → sport (12) → luxury (13).

Text overlay: title, 6 axis labels, 12 min/max values (6 top + 6 bottom), 3 legend labels = 22 labels.

Total: 1 pane, 4 layers, 1 transform, 4 buffers, 4 geometries, 4 drawItems = 18 IDs.

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

- **All 90 line segments are exact.** Every segment for all 18 cars verified against independently computed normalized values. Economy (30/30), Sport (30/30), Luxury (30/30) — zero errors across all 90 segments.

- **Normalization is correct for all 6 axes.** Each value is mapped as (val − min) / (max − min). For example, Economy car 1 MPG=35 → (35−10)/(45−10) = 0.7143. All 108 endpoint values (18 cars × 6 axes) are correct.

- **All 6 axis lines span the full Y range.** Each axis is a vertical line from (i, 0) to (i, 1), evenly spaced at X=0..5.

- **Transform is exact.** X=0→clipX=−0.909, X=5→clipX=0.909. Y=0→clipY=−0.818, Y=1→clipY=0.818. The pane region matches the transform bounds precisely.

- **Group clustering patterns are visually correct.** In the PNG: Economy (green) lines cluster at high MPG (left-high), low cylinders/displacement/HP/weight (middle-low), high acceleration (right-high). Sport (orange) shows mid-range values. Luxury (purple) shows the highest displacement/HP/weight. The characteristic crossing patterns of parallel coordinates are clearly visible.

- **Color coding matches spec.** Economy #34d399→(0.204, 0.828, 0.600) ✓, Sport #f97316→(0.976, 0.451, 0.086) ✓, Luxury #a78bfa→(0.655, 0.545, 0.980) ✓. All hex-to-float conversions verified.

- **Text label positions align with axis positions.** All 6 axis labels and 12 min/max labels are centered (align="c") at clipX values matching the axis X positions through the transform. Max labels at clipY=0.828 (just above pane top), min labels at clipY=−0.858 (just below).

- **All vertex formats correct.** lineAA@1 uses rect4 ✓ for all 4 DrawItems.

- **All vertex counts match.** Axes: 24/4=6 ✓. Economy: 120/4=30 ✓. Sport: 120/4=30 ✓. Luxury: 120/4=30 ✓.

- **Layer ordering is correct.** Axes behind (10), then economy (11), sport (12), luxury (13) on top. This ensures all lines are visible over the subtle axis lines.

- **All 18 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Parallel coordinates require careful normalization.** Each axis has its own data range, but all map to the same [0,1] Y range. The normalization formula (val − min) / (max − min) must be applied per-axis, not globally.

2. **One DrawItem per group is efficient for polylines.** 18 cars × 5 segments = 90 lines, but grouping by color (3 groups of 30) requires only 3 DrawItems. Each DrawItem's buffer concatenates all cars in that group sequentially.

3. **lineAA@1 handles crossing lines naturally.** With 90 segments at alpha 0.7, the many crossings between groups create visual density that reveals cluster patterns. No special handling needed — the alpha blending creates natural visual layering.

4. **Evenly spaced axes simplify the transform.** With axes at integer X positions 0..5, the transform maps linearly to clip space. No per-axis transform needed — one transform handles all axes because the Y normalization is done in the data, not the transform.

5. **The pane region matching the transform bounds exactly is clean.** Unlike charts with margins for axis labels (where the transform maps to a sub-region of the pane), parallel coordinates can use the full pane since text labels go outside the data region.
