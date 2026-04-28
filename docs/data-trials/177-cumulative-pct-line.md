# Data Trial 177: Cumulative % Line (Pareto)
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Pareto analysis — products sorted by revenue, cumulative percentage curve from 0% to 100%. Find the 80/20 point.
**Goal:** Pareto curve showing cumulative revenue contribution by product rank.
**Outcome:** 150 products, 80% of revenue reached at product #75. 10 unique IDs. Zero defects.

---
## What Was Built

Viewport 1000x600. lineAA@1 Pareto curve from 0% to 100%.
Red dashed reference lines mark the 80% threshold horizontally and the crossing product index vertically.

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
- 80% of revenue comes from the top 75 products (out of 150) — 50% of catalog.
- The classic Pareto principle (80/20) is approximately confirmed.

---
## Lessons
1. Pareto curves are straightforward: sort by value, compute cumulative sum, normalize to 100%.
2. Reference lines at key thresholds (80%) provide actionable insight at a glance.
