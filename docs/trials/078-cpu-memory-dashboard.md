# Trial 078: CPU & Memory Dashboard

**Date:** 2026-03-22
**Goal:** 2-pane layout with CPU line chart and memory usage bars sharing a time axis.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

1000x600 viewport with two vertically stacked panes.

1. **CPU pane** (top 50%) -- Green lineAA trace of 24 hourly CPU % readings (20-85%), 23 segments.
2. **Memory pane** (bottom 50%) -- Blue instancedRect bars for 24 hourly memory readings (48-90%), with rounded corners.

Both panes share a linked X-axis time group for synchronized panning.

Total: 9 unique IDs (2 panes, 2 layers, 2 transforms, 1 buffer+geo+DI per pane = 3+3)

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
- **Pane regions.** Top pane [0.05, 1.0], bottom [-1.0, -0.05] with 0.1 gap between them.
- **Transform math.** CPU maps [−1,24]×[10,95] and memory maps [−1,24]×[0,100] into their respective clip regions.
- **LineAA segment continuity.** Each segment endpoint matches the next segment start.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Link viewports for synchronized scroll.** Using linkGroup 'time' ensures both panes pan together on the X axis.
2. **Leave margins around pane regions.** The 0.1 gap between panes provides visual separation.
