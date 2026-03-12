# Trial 043: Arc Diagram

**Date:** 2026-03-12
**Goal:** Social network arc diagram — 10 nodes on a horizontal line connected by 15 semicircular arcs. Short/medium arcs above the line, long arcs below. Tests parametric semicircle tessellation (lineAA@1, 36 segments per arc), triAA@1 node circles, and arc grouping by visual weight (3 tiers with different lineWidth and alpha).
**Outcome:** All 15 arcs are perfect semicircles (max radius error ≤ 0.000001). All 15 connection endpoints match expected node positions. All 10 nodes at correct X positions on Y=0. Above/below classification matches spec. Node circles lack aspect correction (minor). One defect.

---

## What Was Built

A 1000×500 viewport with a single pane (background #0f172a):

**10 node circles (10 triAA@1 DrawItems, pos2_alpha, 144 vertices each):**
At X = 10, 20, 30, ..., 100 on Y=0. Each with a unique categorical color (blue, emerald, amber, pink, violet, cyan, orange, red, lime, purple). Data radius 1.5 units.

**15 connection arcs (3 lineAA@1 DrawItems, rect4, 36 segments per arc):**

| Group | Arcs | Span | LineWidth | Alpha | Direction |
|-------|------|------|-----------|-------|-----------|
| Short | A-C, B-D, D-F, E-G, G-I, H-J | 20 | 2.0 | 0.5 | Above |
| Medium | A-E, B-F, C-G, D-H, E-I, F-J | 40 | 1.5 | 0.4 | Above |
| Long | A-H, B-I, C-J | 70 | 1.0 | 0.3 | Below |

Each arc is a semicircle centered at the midpoint between two nodes. Radius = half the span.

**Horizontal axis line (lineAA@1, rect4, 1 instance):**
From (5, 0) to (105, 0). White, alpha 0.3, lineWidth 1.

Data space: X=[0, 110], Y=[−40, 40]. Transform 2: sx=0.017273, sy=0.02375, tx=−0.95, ty=0.0.

Layers: axis (10) → arcs (11) → nodes (12).

Total: 47 unique IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation.

2. **Node circles not aspect-corrected.** With the non-square viewport (1000×500), px_per_dx=8.636 and px_per_dy=5.938 differ by factor 1.45. The node circles use uniform data-space radius (1.5 units) without correction, resulting in horizontally elongated ellipses: 13.0px at 0° vs 8.9px at 90°. Visible but subtle.

---

## Spatial Reasoning Analysis

### Done Right

- **All 15 arcs are perfect semicircles.** Every arc's maximum radius error is ≤ 0.000001 across all 36+1 tessellation points. No deviations detected.

- **All 15 arc endpoints correctly connect to specified node pairs.** Every arc starts and ends at the correct node X positions (within 1 data unit). All 15/15 connections matched.

- **Above/below classification is correct.** 12 arcs (short + medium spans) above the line, 3 arcs (long spans) below. This matches the spec's even/odd index-sum rule exactly.

- **Visual weight tiers encode connection distance.** Short arcs (span 20) are thickest and most opaque (lw=2, α=0.5), long arcs (span 70) are thinnest and most transparent (lw=1, α=0.3). This creates a natural visual hierarchy.

- **All 10 nodes at correct positions.** X = 10, 20, ..., 100 on Y=0, verified for all 10 circles.

- **Arc tessellation is smooth.** 36 segments (5° each) per semicircle produces visually smooth curves at 1000×500 resolution.

- **Long arcs fit within data bounds.** Largest arc (span 70, radius 35) reaches Y=−35, within the Y=[−40, 40] range.

- **Layering is correct.** Axis (back) → arcs (middle) → nodes (front). Nodes draw over arc endpoints, creating clean termination points.

- **Transform math is correct.** X=[0,110] and Y=[−40,40] map correctly to clip[−0.95, 0.95]. The ty=0 correctly centers the Y=0 axis line at clip-space Y=0.

- **All vertex formats correct.** lineAA@1 uses rect4 ✓, triAA@1 uses pos2_alpha ✓.

- **All vertex counts match.** Short: 864/4=216 ✓. Medium: 864/4=216 ✓. Long: 432/4=108 ✓. Each node: 144 verts ✓.

- **All 47 IDs unique.** Non-standard allocation (transform 2, DrawItems 300-series) but no collisions.

### Done Wrong

- **Node circles lack aspect correction.** The 1000×500 viewport creates a 1.45:1 pixel aspect ratio. Without per-axis radius adjustment, circles appear as horizontal ellipses (~13px wide, ~9px tall). The fix: X_data_radius = target_px / px_per_dx, Y_data_radius = target_px / px_per_dy.

---

## Lessons for Future Trials

1. **Arc diagrams place nodes on a line and connections as semicircles.** The arc center is the midpoint between connected nodes, with radius equal to half their distance. This creates naturally nested arcs — short connections are small arcs close to the axis, long connections are large arcs that arch high.

2. **Above/below alternation reduces visual clutter.** Splitting arcs by direction (based on some criterion like span or index parity) distributes the visual weight across both sides of the axis, preventing one side from becoming an overlapping mess.

3. **Non-square viewports require aspect correction for circles.** With a 2:1 aspect ratio (1000×500), circles must have different X and Y data radii to appear circular in pixel space. This trial omitted this correction, producing visible ellipses.

4. **Grouping arcs by visual weight minimizes DrawItems.** Instead of one DrawItem per arc (15 items), grouping by span into 3 tiers (short/medium/long) with different lineWidth and alpha produces 3 DrawItems that still convey connection distance.

5. **36 line segments per semicircle is sufficient.** At 5° per segment, the maximum chord error for a 35-unit radius arc is ~0.03 data units (< 1 pixel). Smaller arcs need even fewer segments but using 36 uniformly is simpler and still efficient.
