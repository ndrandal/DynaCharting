# Trial 096: League Table

**Date:** 2026-03-22
**Goal:** 8 horizontal bars showing team standings by points.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

800x600 viewport with one pane.

Eight horizontal bars representing team standings sorted by points: Lions (82), Eagles (76), Tigers (71), Bears (65), Wolves (58), Hawks (52), Sharks (45), Ravens (38). All bars share one color (blue). Bar length proportional to points (out of 100). Rounded corners (3px).

Total: 4 unique IDs (1 pane, 1 layer, 1 buf+geo+DI group = 3)

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
- **Uniform color.** All bars share one draw item with a single color — simpler than per-team colors.
- **Consistent spacing.** 0.225 clip units per row, 0.18 bar height, 0.045 gap.
- **Proportional lengths.** Points out of 100 mapped directly to clip width fraction.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Batch same-color rects.** All 8 bars fit in one buffer and one draw item since they share a color.
2. **Use rounded corners for polish.** Even small cornerRadius (3px) improves readability.
