# Trial 099: Market Treemap

**Date:** 2026-03-22
**Goal:** 12 rectangles representing market sectors, colored by category.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

900x700 viewport with one pane.

12 rectangles in a 3×4 grid layout, sorted by market cap (descending). Four color categories: Tech (blue, 3 items), Finance (green, 2), Health (orange, 3), Energy (yellow, 4). Items sorted by value from top-left (largest) to bottom-right (smallest). Rounded corners (4px), small gaps between cells.

Total: 15 unique IDs (1 pane, 1 layer, 4×(buf+geo+di)=12, 0 transforms)

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
- **Grid packing.** 3×4 grid with 0.02 gap fills the viewport efficiently.
- **Color grouping.** Same-category rects batched into one draw item each.
- **Sort order.** Descending by value puts most important items at top-left.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Simple grid approximates treemap.** True squarification is complex; a sorted grid conveys the same information.
2. **Group by category for color coding.** Each draw item gets one color, so batch by category.
