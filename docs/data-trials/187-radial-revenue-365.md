# Data Trial 187: Radial Revenue 365
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** 365 days arranged radially like a clock. Each day is a wedge whose length encodes revenue. Requires trigonometric layout for all 365 wedges.
**Goal:** Full-year revenue as a radial bar chart (sunburst-style).
**Outcome:** 365 wedges, 2190 vertices. Max daily revenue: $7,432. 5 unique IDs. Zero defects.

---
## What Was Built

Viewport 800x800 (square). 365 triSolid@1 wedges radiating from a center hole (r=0.15).
Wedge length proportional to daily revenue. Layout clockwise from top (Jan).

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
- 365 wedges create a dense radial pattern — seasonal patterns appear as radius variation.
- The central hole prevents tiny wedges near the origin from becoming invisible.

---
## Lessons
1. Radial charts require trigonometric coordinate computation but the result is visually striking.
2. At 365 wedges, individual days become ~1-degree slices — pattern recognition trumps precision reading.
