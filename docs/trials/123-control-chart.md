# Trial 123: Control Chart (SPC)

**Date:** 2026-03-22
**Goal:** Statistical process control chart with 30 data points (lineAA@1 + points@1) and UCL/LCL/mean dashed reference lines.
**Outcome:** SPC control chart with 30 data points (mean=50, sigma=3). Zero defects.

---

## What Was Built
Viewport 800x500. SPC control chart with 30 data points (mean=50, sigma=3). Data shown as connected lineAA@1 line with point markers (points@1). UCL (59) and LCL (41) as red dashed lines, mean (50) as green dashed line. One out-of-control point injected at index 22 (~60.5).

| DrawItem | Layer | Element | Pipeline | Count | Color |
|---|---|---|---|---|---|
| 102 | 11 | Data line | lineAA@1 | 29 seg | cyan |
| 105 | 11 | Data points | points@1 | 30 pts | white |
| 108 | 10 | UCL | lineAA@1 | 1 seg | red |
| 111 | 10 | LCL | lineAA@1 | 1 seg | red |
| 114 | 10 | Mean | lineAA@1 | 1 seg | green |

Total: 19 unique IDs (1 pane, 2 layers, 1 transform, 5 buffers, 5 geometrys, 5 drawItems).

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
- **UCL and LCL at mean +/- 3 sigma (59 and 41) correctly bracket 99.7% of expected variation.** 
- **Out-of-control point at index 22 is visibly above the UCL, demonstrating the detection purpose of SPC charts.** 
- **Data points are rendered on top of the connecting line (same layer, higher ID) for clear visibility.** 
- **Control limits rendered behind data (layer 10 < 11) so data line is not occluded by dashed lines.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **SPC charts use mean +/- 3 sigma for control limits, covering 99.7% of normal variation.** 
2. **Overlaying points@1 on lineAA@1 with the same transform gives dual representation — line shows trend, points show individual values.** 
