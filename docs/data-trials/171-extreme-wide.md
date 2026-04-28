# Data Trial 171: Extreme Wide
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** 2000x200 viewport — extreme aspect ratio (10:1). Tests line rendering with minimal vertical space.
**Goal:** Monthly revenue as a sparkline in an extremely wide, short viewport.
**Outcome:** 20 line segments in 2000x200 viewport. 6 unique IDs. Zero defects.

---
## What Was Built

Viewport 2000x200. lineAA@1 sparkline with line width 2.5px.
At this aspect ratio, each month spans ~95 pixels horizontally but the entire Y range is only ~170px.

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
- Revenue trend is visible as a gentle sparkline pattern.
- The extreme width makes small variations more visible than tall charts.

---
## Lessons
1. Extreme aspect ratios render correctly — the transform handles arbitrary viewport dimensions.
2. Sparkline-style charts benefit from wider viewports that spread the time axis.
