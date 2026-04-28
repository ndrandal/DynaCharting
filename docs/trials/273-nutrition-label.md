# Trial 273: Nutrition Label

**Date:** 2026-03-22
**Goal:** Structured nutrition label layout with border, header area, thick/thin dividers, and % daily value bars for 12 nutrients.
**Outcome:** 4-segment border, 1 header rect, 3 thick dividers, 11 thin dividers, 11 DV bars. 19 unique IDs. Zero defects.

---

## What Was Built

Viewport 400x700. Single pane with near-black background.

**5 DrawItems across 3 layers:**

| DrawItem | Layer | Element | Pipeline | Count | Color |
|----------|-------|---------|----------|-------|-------|
| 102 | 10 | Border | lineAA@1 | 4 segs | white, lw=3 |
| 105 | 10 | Header bg | instancedRect@1 | 1 rect | dark gray |
| 108 | 11 | Thick dividers | lineAA@1 | 3 segs | light gray, lw=2.5 |
| 111 | 11 | Thin dividers | lineAA@1 | 11 segs | gray, lw=0.8 |
| 114 | 12 | DV bars | instancedRect@1 | 11 rects | blue |

Label box: [-0.65, -0.85] to [0.65, 0.85]. Rows: 12 nutrients, each 0.1042 tall.

Total: 19 unique IDs (1 pane, 3 layers, 5 buffers, 5 geometries, 5 drawItems).

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
- **Hierarchical divider thickness.** Thick lines separate major sections (header, calories, footer); thin lines separate nutrient rows.
- **DV bars proportional to values.** Higher % daily value = wider bar, easy to compare nutrients at a glance.
- **Header area provides visual anchor.** Darker rectangle at top marks the "Nutrition Facts" area.
- **Row spacing is uniform.** All 12 nutrients equally spaced within the available area.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Varying line thickness creates visual hierarchy.** 3.0 for border, 2.5 for major dividers, 0.8 for row dividers.
2. **Structured form layouts.** Divide available space into header, body rows, and footer using arithmetic positioning.
