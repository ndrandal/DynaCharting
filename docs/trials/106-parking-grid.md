# Trial 106: Parking Grid

**Date:** 2026-03-22
**Goal:** 6x8 grid of parking spots with availability colors.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

800x500 viewport with one pane.

48 parking spots in a 6-row × 8-column grid. Available: 22 (green), Occupied: 18 (red), Reserved: 8 (gray). Small gaps between spots. Background color suggests asphalt. Rounded corners (3px).

Total: 12 unique IDs (1 pane, 1 layer, 3×(buf+geo+di)=9, 0 transforms)

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
- **Grid dimensions.** 8 columns × 6 rows with 0.01 clip-unit gaps.
- **Color semantics.** Green/red/gray matches universal parking signage conventions.
- **Batch by status.** 48 rects in 3 draw calls.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Use familiar color conventions.** Green=available, red=taken is universally understood.
2. **Asphalt background color.** Dark gray-blue [0.1, 0.12, 0.18] evokes a parking lot surface.
