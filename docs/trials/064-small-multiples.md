# Trial 064: Small Multiples

**Date:** 2026-03-12
**Goal:** 3×3 grid of 9 mini line charts (small multiples), each showing a different mathematical function. Tests multi-pane layout at scale (9 panes with non-overlapping regions), per-pane transforms with auto-fitted Y ranges, and consistent styling across a grid on a 900×900 viewport.
**Outcome:** All 9 functions match expected formulas with 0.000000 average error. All 9 panes non-overlapping. All 90 IDs unique. Zero defects.

---

## What Was Built

A 900×900 viewport (square) with 9 panes in a 3×3 grid (background #0f172a each):

**9 mathematical functions (9 lineAA@1 DrawItems, rect4, 49 segments each):**

| Row | Col 0 (blue) | Col 1 (emerald) | Col 2 (amber) |
|-----|-------------|-----------------|---------------|
| 0 (top) | sin(x) | cos(x) | sin(x)·cos(x/2) |
| 1 (mid) | e^(−x/3)·sin(2x) | x·sin(x) | sin(x²/5) |
| 2 (bot) | |sin(3x)| | tanh(x−5) | sin(x)+sin(2x)/2+sin(3x)/3 |

Each function sampled at 50 points over x ∈ [0, 10]. Colors cycle by column: blue #3b82f6, emerald #10b981, amber #f59e0b. LineWidth 2.0, alpha 0.9.

**9 zero-reference lines (9 lineAA@1 DrawItems, rect4, 1 segment each):**
White, alpha 0.1, lineWidth 1.0. One per pane at Y=0, behind the curve.

**Per-pane transforms:** Each pane has its own transform mapping X=[0,10] and Y=[ymin,ymax] (auto-fitted with 10% padding) to the pane's clip region.

Grid layout: cell size ≈ 0.6133 clip units, gap = 0.02 clip units.

Total: 90 unique IDs (9 panes, 18 layers, 9 transforms, 18 buffers, 18 geometries, 18 drawItems).

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

- **All 9 functions match expected formulas exactly.** Each curve was matched against all 9 functions by computing average absolute error. All 9 best matches had 0.000000 error, confirming every data point is numerically exact.

- **All 9 panes are non-overlapping.** Every pair of panes was tested for clip-space overlap. None found. The 0.02-unit gaps between cells create visible dark separators.

- **Per-pane auto-fitted Y ranges.** Each function has its own Y range with 10% padding. The "Growing" function (x·sin(x)) with range ~[−6.8, 9.3] gets a very different Y scale than "Sigmoid" (tanh, range [−1.2, 1.2]). Each chart fills its pane.

- **Color cycling by column.** Blue (col 0), emerald (col 1), amber (col 2) creates visual grouping by column while maintaining readability.

- **Zero-reference lines on lower layers.** The subtle white lines at Y=0 in each pane help read positive/negative values without cluttering the visualization.

- **9 visually distinct function shapes.** Sine (regular oscillation), sigmoid (S-curve), chirp (accelerating frequency), damped (decaying amplitude), growing (increasing amplitude), etc. Each chart is immediately distinguishable.

- **49 segments per curve provide smooth rendering.** At ~300px per cell, each segment spans ~6px — sufficient for smooth curves.

- **All vertex formats correct.** lineAA@1 uses rect4 ✓.

- **All buffer sizes match vertex counts.** 18/18 geometries verified.

- **All 90 IDs unique.** No collisions across 9 panes, 18 layers, 9 transforms, 18 buffers, 18 geometries, 18 drawItems.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Small multiples require per-pane transforms.** Each chart needs its own transform to map its unique Y range to its pane's clip region. The X range can be shared if all charts have the same domain.

2. **Auto-fitted Y ranges with 10% padding prevent data from touching pane edges.** Computing [min−pad, max+pad] per function ensures all data is visible with breathing room.

3. **3×3 grid layout with gaps: cell_size = (total − 2×gap) / 3.** For 1.88 clip units of usable space and 0.02 gaps, each cell is 0.6133 units. The gap creates visual separation without wasting much space.

4. **Color cycling by column (not by function) creates visual grouping.** Functions in the same column share a color, making the grid structure immediately apparent.

5. **90 IDs for 9 charts is manageable with systematic allocation.** Panes 1–9, layers 10+2i/11+2i, transforms 50–58, buffers/geometries/drawItems in 100s/200s/300s ranges with curve/zero-line offsets.
