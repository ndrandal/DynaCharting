# Data Trial 183: Butterfly Chart
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Butterfly/tornado chart with bars extending left (expenses) and right (revenue) from a center axis.
**Goal:** Monthly revenue vs expenses as opposing horizontal bars.
**Outcome:** 21 months x 2 sides. Revenue bars right (green), expense bars left (red). 13 unique IDs. Zero defects.

---
## What Was Built

Viewport 1000x700. Two instancedRect@1 DrawItems sharing one transform.
Revenue bars extend right from zero; expense bars extend left (negative X) from zero.
Vertical zero line as lineAA@1.

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
- Expenses consistently exceed revenue — the hardware store operates at a loss by this metric.
- The butterfly layout makes the imbalance immediately visible.

---
## Lessons
1. Butterfly charts are just horizontal bars with negative X values for the left side.
2. A shared transform with symmetric X range ensures both sides are comparable.
