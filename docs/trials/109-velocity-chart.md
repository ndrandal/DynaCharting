# Trial 109: Velocity Chart

**Date:** 2026-03-22
**Goal:** Sprint velocity bars with 3-period moving average line overlay.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

900x500 viewport with one pane and two layers.

Eight bars showing sprint velocities (35-55 story points). Orange moving average line (3-period window) overlaid on the bars. Bars on layer 10, average on layer 11.

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
- **Bar-line overlay.** Bars behind moving average line via layer ordering.
- **Moving average.** 3-period window smooths the velocity trend.
- **Shared transform.** Both bars and line use transform 50 for consistent mapping.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Moving averages on top of bars.** Layer 11 (line) over layer 10 (bars) is the standard pattern.
2. **Use window-based averaging for trend lines.** 3-period window balances smoothing vs responsiveness.
