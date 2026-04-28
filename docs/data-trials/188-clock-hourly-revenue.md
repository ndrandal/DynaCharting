# Data Trial 188: Clock Hourly Revenue
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Revenue by hour plotted on a 24-hour clock face. Radial bars extend outward from center for each hour.
**Goal:** Clock-face visualization of hourly revenue distribution.
**Outcome:** 14 hour wedges on clock face. Peak hour: 14:00 ($174,505). 9 unique IDs. Zero defects.

---
## What Was Built

Viewport 700x700 (square). 24-hour clock with triSolid@1 wedges.
Hour 0 at top, clockwise. Wedge length proportional to revenue.
lineAA@1 tick marks at each hour, labels at 0, 6, 12, 18.

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
- Peak revenue at 14:00 — the busiest hour for the hardware store.
- Minimal activity before 7am and after 8pm — typical retail pattern.
- The clock layout makes the "busy window" immediately obvious.

---
## Lessons
1. Clock-face layouts map hours to angles (hour/24 * 2pi) with 12 o'clock at the top.
2. Multiple sub-segments per wedge (3) smooth the angular edges.
