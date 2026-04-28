# Data Trial 186: Sankey Approximation
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Sankey/alluvial diagram is impossible with basic primitives. Approximation: left bars (departments), right bars (tiers), connecting bands (triSolid@1 quadrilaterals).
**Goal:** Visualize revenue flow from 8 departments to 3 customer tiers.
**Outcome:** 8 dept bars + 3 tier bars + connecting bands. 12 unique IDs. Zero defects.

---
## What Was Built

Viewport 1000x700. Three layers:
- triSolid@1 translucent bands connecting department outputs to tier inputs
- instancedRect@1 bars on left (departments, blue) and right (tiers, orange)

Bands are straight-sided (no curves) — a simplification of true Sankey diagrams.

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
- All departments serve all three tiers, but gold customers generate the most revenue per tier.
- Tools & Hardware flows to all tiers roughly proportionally.

---
## Lessons
1. Sankey diagrams can be approximated with straight-sided triSolid@1 bands between bar endpoints.
2. The approximation lacks curves but still communicates flow relationships effectively.
