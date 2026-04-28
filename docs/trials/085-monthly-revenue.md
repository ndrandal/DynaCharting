# Trial 085: Monthly Revenue

**Date:** 2026-03-22
**Goal:** Dual-axis chart with revenue bars and dashed trend line overlaid in a single pane.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

1000x500 viewport with one pane and two layers.

1. **Bar layer** -- 12 blue instancedRect bars (one per month) showing revenue ($42K-$85K), rounded corners.
2. **Trend layer** -- Yellow dashed lineAA trend line (11 segments) overlaid on top of bars.

Both layers share the same transform for aligned data mapping.

Total: 9 unique IDs (1 pane, 2 layers, 1 transform, 2 buf+geo+DI groups)

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
- **Layer ordering.** Bar layer (ID 10) renders behind trend layer (ID 11), so the line overlays the bars.
- **Shared transform.** Both draw items use transform 50 for consistent X and Y mapping.
- **Dashed line.** dashLength=10.0 and gapLength=6.0 create a visually distinct trend indicator.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Use multiple layers for overlay compositing.** Bars on layer 10, trend on layer 11 ensures correct z-order.
2. **Dashed lines distinguish reference data.** The trend line's dash pattern separates it from actual revenue.
