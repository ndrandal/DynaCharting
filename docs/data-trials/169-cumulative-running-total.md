# Data Trial 169: Cumulative Running Total
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Growing area fill from day 1 to day 630 — the curve climbs monotonically from 0 to $1,328,671.
**Goal:** Cumulative revenue from first to last day as triSolid@1 filled area + lineAA@1 top edge.
**Outcome:** Area fill (3774 vertices) + line (629 segments). Final total: $1,328,671. 10 unique IDs. Zero defects.

---
## What Was Built

Viewport 1200x600. Two layers: translucent green area fill (alpha 0.3) + solid green line on top.
The cumulative curve is smooth and monotonically increasing.

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
- Total cumulative revenue: $1,328,671 over 630 days.
- Growth is roughly linear — consistent daily revenue with no major structural shifts.

---
## Lessons
1. Cumulative charts combine triSolid@1 area fill with lineAA@1 overlay for a polished look.
2. Large triSolid@1 vertex counts (3774) from area tessellation work fine.
