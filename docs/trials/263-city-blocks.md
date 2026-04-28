# Trial 263: City Blocks

**Date:** 2026-03-22
**Goal:** 4x3 city block grid with streets as gaps, parks in green, key buildings in blue. Tests grid layout with varying block types.
**Outcome:** 12 blocks (8 normal + 2 parks + 2 highlighted), street grid lines. 15 unique IDs. Zero defects.

---

## What Was Built

Viewport 800x600. Single pane with dark urban background.

**4 DrawItems across 2 layers:**

| DrawItem | Layer | Element | Pipeline | Count | Color |
|----------|-------|---------|----------|-------|-------|
| 102 | 10 | Normal blocks | instancedRect@1 | 8 rects | gray |
| 105 | 10 | Parks | instancedRect@1 | 2 rects | green |
| 108 | 10 | Key buildings | instancedRect@1 | 2 rects | blue |
| 111 | 11 | Streets | lineAA@1 | 9 segs | tan |

Block size 0.38x0.42 in clip space. Streets are 0.06 wide gaps with center lines.

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
- **Blocks evenly spaced with street gaps.** 0.44 pitch with 0.38 block = 0.06 street width.
- **Parks and key buildings visually distinct.** Green for parks, blue for highlighted buildings.
- **Street grid extends slightly beyond blocks.** Lines overshoot to create realistic intersection look.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Gaps between rects create implicit streets.** No need to draw streets as filled rects; the background shows through.
2. **Color-coding block types makes the map readable.** Three categories with three colors.
