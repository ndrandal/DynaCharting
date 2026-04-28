# Data Trial 010: Payment Method Donut Chart
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** payment_method_breakdown()
**Goal:** Donut chart showing revenue share by payment method with inner cutout.
**Outcome:** Clean donut with center hole. Zero defects.
---
## What Was Built
Viewport 600x600. triSolid@1 pipeline. 4 ring segments, 210 total vertices.
Outer radius 0.8, inner radius 0.45.
Total: 14 unique IDs.
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
- Same data as trial 009 but donut form factor emphasizes proportions over area.
- credit_card at 51.1% remains the dominant method.
---
## Lessons
1. to_donut_wedges generates 2 triangles per sub-segment (outer-inner ring), doubling vertex count vs pie.
