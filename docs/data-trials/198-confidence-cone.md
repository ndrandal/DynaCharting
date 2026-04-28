# Data Trial 198: Confidence Cone
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Forward projection with widening confidence band — the cone expands linearly with distance from the last known data point.
**Goal:** Historical revenue line + 6-month forward projection with uncertainty band.
**Outcome:** 20 historical segments + 6 projection segments + confidence cone. 17 unique IDs. Zero defects.

---
## What Was Built

Viewport 1100x600. Three visual elements:
- Blue solid line: historical monthly revenue
- Orange dashed line: projected revenue (linear extrapolation)
- Orange translucent cone: widening confidence band (1 std + 0.5*std per month)

Vertical dashed line marks the boundary between history and projection.

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
- Linear projection assumes trend continues unchanged — a simplistic but useful baseline.
- The widening cone honestly communicates increasing uncertainty over time.

---
## Lessons
1. Confidence cones are triSolid@1 bands between upper and lower bounds that widen with extrapolation distance.
2. Dashed projection lines visually signal "forecast" vs "actual."
