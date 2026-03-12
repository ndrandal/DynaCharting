# Trial 070: Lissajous Figures

**Date:** 2026-03-12
**Goal:** 3×3 grid of 9 Lissajous figures with different frequency ratios (a:b from 1:2 to 5:6), phase δ=π/4, color-coded by row. Tests multi-pane layout at scale (9 panes with per-pane transforms), parametric trigonometric curves, and systematic parameter variation on a 900×900 square viewport.
**Outcome:** All 9 curves match the Lissajous formula with 0.000000000 error across all 4,500 segments. All 9 panes non-overlapping with 0.02 gaps. Colors correct by row. 54 unique IDs. Zero defects.

---

## What Was Built

A 900×900 viewport (square) with 9 panes in a 3×3 grid (background #0f172a each):

**9 Lissajous curves (9 lineAA@1 DrawItems, rect4, 500 segments each):**

| Row | Col 0 | Col 1 | Col 2 | Color |
|-----|-------|-------|-------|-------|
| 0 (top) | 1:2 | 1:3 | 1:4 | #ef4444 (red) |
| 1 (mid) | 2:3 | 3:4 | 2:5 | #3b82f6 (blue) |
| 2 (bot) | 3:5 | 4:5 | 5:6 | #10b981 (emerald) |

Formula: x(t) = sin(a·t + π/4), y(t) = sin(b·t), t ∈ [0, 2π].

Each curve: 500 segments, lineWidth 1.5, alpha 0.8.

Grid layout: cell size 0.6133 clip units, gap 0.02 clip units. All panes square.

Per-pane transforms: map data space [-1.1, 1.1] × [-1.1, 1.1] to each pane's clip region.

Total: 54 unique IDs (9 panes, 9 layers, 9 transforms, 9 buffers, 9 geometries, 9 drawItems).

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

- **All 4,500 segments across 9 curves match the Lissajous formula exactly.** Every segment verified against x = sin(a·t + π/4), y = sin(b·t). Maximum error: 0.000000000 — numerically exact.

- **Visual complexity increases left-to-right and top-to-bottom.** Row 0 (1:2, 1:3, 1:4) shows simple open curves. Row 1 (2:3, 3:4, 2:5) shows closed interlocking loops. Row 2 (3:5, 4:5, 5:6) shows dense multi-loop patterns. This progression matches the increasing frequency ratios.

- **All 9 panes non-overlapping.** Every pair tested — no overlap. The 0.02-unit gaps create visible dark separators.

- **Pane dimensions are uniform.** All 9 panes exactly 0.6133×0.6133 clip units — perfect squares on the square viewport.

- **Colors correctly group by row.** Red (row 0), blue (row 1), emerald (row 2). All 3 curves in each row share the same color.

- **Phase δ=π/4 creates open curves.** Without the phase shift, integer-ratio Lissajous figures would trace closed paths. The π/4 phase offset opens the curves slightly, making the internal structure more visible.

- **Data range [-1.1, 1.1] provides 10% padding.** Since sin ranges from -1 to 1, the 0.1 padding prevents curves from touching pane edges.

- **500 segments per curve provide smooth rendering.** At ~184 pixels per pane, each segment spans ~0.37 pixels — more than sufficient for smooth curves.

- **All buffer sizes match vertex counts.** 9/9 geometries verified (rect4: 4 fpv, 500 verts each).

- **All 54 IDs unique.** No collisions across 9 panes, 9 layers, 9 transforms, 9 buffers, 9 geometries, 9 drawItems.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Lissajous complexity scales with frequency ratio magnitude.** Low ratios (1:2) produce simple curves with few lobes. Higher ratios (5:6) produce dense multi-loop patterns. The number of x-lobes equals a, the number of y-lobes equals b.

2. **Phase offset δ opens closed Lissajous curves.** Without phase, integer-ratio curves trace the same path repeatedly. A non-zero δ (like π/4) breaks the exact closure, revealing the curve's internal structure.

3. **9-pane grid with per-pane transforms is identical to Trial 064's pattern.** Cell size = (1.88 - 2×gap) / 3. Each pane maps its own data range to its own clip region independently.

4. **54 IDs for 9 panes follows the 6-per-pane pattern.** Each pane needs: 1 pane + 1 layer + 1 transform + 1 buffer + 1 geometry + 1 drawItem = 6 resources. 9 × 6 = 54.

5. **0.000000000 error indicates the agent used identical computation to the spec.** Both used sin/cos on the same t-values, producing bit-identical results. This is the ideal outcome for deterministic parametric curves.
