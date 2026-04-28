# Trial 086: Real Estate Comparison

**Date:** 2026-03-22
**Goal:** Grouped bar chart comparing 4 properties across 3 metrics.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

900x500 viewport with one pane.

12 bars in 4 groups of 3, comparing Downtown, Suburb, Beach, and Mountain properties. Three metrics: Price (blue, $280K-$520K), SqFt (green, 1600-3000), and Score (orange, 72-90). Bars use a shared transform mapping data space to clip space. Rounded corners (2px).

Total: 12 unique IDs (1 pane, 1 layer, 1 transform, 3×(buf+geo+di)=9)

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
- **Group spacing.** Properties spaced 1.0 apart in data space with 3 narrow bars (0.12 wide) per group.
- **Color-coded metrics.** Blue/green/orange distinctly identifies each metric within a group.
- **Unified Y axis.** All three metrics share the same Y range (0-550), allowing direct visual comparison.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Group bars by metric for consistent coloring.** One draw item per metric color simplifies the scene.
2. **Normalize data carefully.** All three metrics share one Y axis, so values must be on comparable scales.
