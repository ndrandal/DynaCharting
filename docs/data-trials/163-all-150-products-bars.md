# Data Trial 163: All 150 Products Bars
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Stress test with 150 bars — can the engine render many narrow bars without visual artifacts?
**Goal:** ALL 150 products as bars sorted by revenue (descending), in a wide viewport.
**Outcome:** 150 instancedRect@1 bars. Top product: Cordless Drill/Driver 20V Kit ($77,152.32). Bottom: R-13 Insulation Batt 15in ($215.49). 6 unique IDs. Zero defects.

---
## What Was Built

Viewport 1600x600 (wide). Single pane, 150 bars packed tightly.
Interactive viewport for pan/zoom — essential since 150 bars in 1600px means ~10px per bar.

Products sorted by revenue descending. Each bar is 0.7 data units wide with 0.3 unit gaps.

---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---
## Data Insights
- Huge revenue disparity: top product $77,152.32, bottom $215.49 (0.3% of top).
- Revenue follows a power-law-like distribution: top 10% of products account for a disproportionate share.

---
## Lessons
1. 150 bars in one pane is feasible but requires wide viewport or interactive pan/zoom.
2. instancedRect@1 handles large instance counts efficiently — 150 rects is trivial for the GPU.
