# Trial 084: Server Status Grid

**Date:** 2026-03-22
**Goal:** 4x4 grid of colored server status indicators.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

600x600 viewport with a single pane on dark background.

16 server status squares in a 4x4 grid. Colors indicate status: green (12 servers OK), yellow (2 warnings), red (1 critical). Squares grouped by color into 3 draw items for rendering efficiency. Rounded corners (4px).

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
- **Grid layout.** 4x4 arrangement with 0.45 clip-unit spacing and 0.38-wide squares gives 0.07 gaps.
- **Color grouping.** Batching rects by status color reduces draw calls from 16 to 3.
- **Status distribution.** 12 green, 2 yellow, 1 red represents a realistic server fleet.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Group same-color rects into one draw item.** instancedRect@1 applies one color, so grouping by color is efficient.
2. **Use distinctive status colors.** Green/yellow/red is universally understood for health indicators.
