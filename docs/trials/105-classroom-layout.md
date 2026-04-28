# Trial 105: Classroom Layout

**Date:** 2026-03-22
**Goal:** 5x5 grid of desk rectangles showing occupied and empty seats.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

600x600 viewport with one pane.

25 desks in a 5×5 grid. 16 occupied (blue), 9 empty (gray). Each desk is 0.28×0.28 clip units with 0.07 gaps between them. Rounded corners (4px). Two draw items — one for occupied, one for empty.

Total: 8 unique IDs (1 pane, 1 layer, 2×(buf+geo+di)=6, 0 transforms)

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
- **Grid alignment.** 5×5 layout with 0.35 spacing fits within [-0.8, 0.95] clip range.
- **Color distinction.** Blue (occupied) vs gray (empty) is immediately readable.
- **Batching.** 25 rects rendered in 2 draw calls by grouping by status.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Batch by visual state.** Grouping occupied/empty into two draw items is simpler than 25 individual items.
2. **Use semi-transparency for empty states.** Alpha 0.6 on empty desks makes them visually recede.
