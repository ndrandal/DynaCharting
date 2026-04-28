# Trial 264: Subway Map

**Date:** 2026-03-22
**Goal:** Transit diagram with 3 colored lines (red, blue, green), 14 station dots, and 3 transfer stations (larger circles with inner cutout).
**Outcome:** 3 lines with lineWidth=4, 11 regular stations (R=0.025), 3 transfer stations (R=0.045 outer, R=0.025 inner cutout). 23 unique IDs. Zero defects.

---

## What Was Built

Viewport 700x700. Single pane with dark background.

**6 DrawItems across 4 layers:**

| DrawItem | Layer | Element | Pipeline | Count | Color |
|----------|-------|---------|----------|-------|-------|
| 102 | 10 | Red line | lineAA@1 | 5 segs | red, lw=4 |
| 105 | 10 | Blue line | lineAA@1 | 5 segs | blue, lw=4 |
| 108 | 10 | Green line | lineAA@1 | 4 segs | green, lw=4 |
| 111 | 11 | Regular stations | triSolid@1 | 132 tris | white |
| 114 | 12 | Transfer outer | triSolid@1 | 48 tris | white |
| 117 | 13 | Transfer inner | triSolid@1 | 36 tris | background |

Transfer stations appear as white rings (outer circle with dark inner circle).

Total: 23 unique IDs (1 pane, 4 layers, 6 buffers, 6 geometries, 6 drawItems).

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
- **Three lines follow distinct paths.** Red horizontal, blue diagonal, green vertical-ish. They cross at 3 shared points.
- **Transfer stations are larger with inner cutout.** The dark inner circle creates a ring effect showing interchange points.
- **Layer ordering correct.** Lines behind stations behind transfer rings.
- **Station deduplication.** Each physical station appears only once regardless of how many lines pass through.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Transfer station ring effect.** Draw a large white circle, then a smaller dark circle on a higher layer to create a ring.
2. **Set deduplication for shared stations.** Using a set prevents duplicate circles where lines cross.
