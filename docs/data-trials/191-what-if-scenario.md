# Data Trial 191: What-If Scenario
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Scenario modeling — compute impact of boosting top-10 products by 20%. Two overlaid lines: actual vs hypothetical.
**Goal:** Compare actual monthly revenue with "what if top 10 products had 20% more sales."
**Outcome:** Two lineAA@1 lines diverging by ~$2,881/month average. 9 unique IDs. Zero defects.

---
## What Was Built

Viewport 1000x600. Gray solid line (actual) + green dashed line (what-if scenario).
The dashed pattern visually signals "hypothetical/projected."

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
- Boosting top-10 products by 20% adds ~$2,881/month on average.
- The gap between lines shows the potential revenue opportunity.

---
## Lessons
1. Scenario/what-if charts use dashed lines to distinguish hypothetical from actual.
2. The analysis requires joining sale_items to monthly periods — Python handles the aggregation.
