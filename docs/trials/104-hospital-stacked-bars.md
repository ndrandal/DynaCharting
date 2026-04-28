# Trial 104: Hospital Stacked Bars

**Date:** 2026-03-22
**Goal:** 5 departments × 4 bed occupancy categories as stacked bars.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

900x500 viewport with one pane.

Five stacked bars (one per department: ER, ICU, Surgery, Pediatric, General), each subdivided into 4 categories:
- Red: Occupied beds
- Yellow: Reserved
- Green: Available
- Gray: Maintenance

Each bar totals 100%. 20 rectangles total (4 per bar). Shared transform.

Total: 15 unique IDs (1 pane, 1 layer, 1 transform, 4×(buf+geo+di)=12)

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
- **Stacking.** Categories stack bottom-to-top: Occupied, Reserved, Available, Maintenance.
- **Consistent totals.** Each department sums to 100, so all bars reach the same height.
- **Category batching.** Same-category rects across all departments batch into one draw item.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Stack from bottom up with cumulative Y base.** Each category starts where the previous ended.
2. **Batch by category, not by department.** One draw item per color is more efficient than one per bar.
