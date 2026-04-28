# Trial 253: Warehouse Grid

**Date:** 2026-03-22
**Goal:** 8x6 warehouse bin grid (48 bins) color-coded by status: green=in-stock, yellow=low, red=empty. Dashed aisles.
**Outcome:** 24 green, 10 yellow, 14 red bins = 48 total. 2 aisle lines. 15 unique IDs. Zero defects.

---

## What Was Built
Viewport 800x600. Dark background. 48 bins in 8x6 grid with cornerRadius=3.
Status distribution (seed=42): 24 in-stock (green), 10 low (yellow), 14 empty (red).
Dashed aisle lines bisect grid horizontally and vertically.
Total: 15 unique IDs (1 pane, 2 layers, 4 buffers, 4 geometries, 4 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **48 bins fill grid uniformly.** Equal spacing with 0.01 gap between bins.
- **Color coding immediately readable.** Traffic-light convention: green=good, yellow=warning, red=critical.
- **Aisles bisect grid into quadrants.** Standard warehouse layout.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Heatmap/status grids are ideal for instancedRect@1.** Each bin = 1 rect instance, grouped by color.
2. **Separate buffers per color avoids needing per-vertex color.** 3 drawItems with different colors.
