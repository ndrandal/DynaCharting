# Data Trial 176: Percentage Stacked Area
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** 8 department areas stacked to 100% per month. Requires computing per-month percentages and building consecutive triangulated bands.
**Goal:** Department revenue share over time, normalized to 100% each month.
**Outcome:** 8 stacked triSolid@1 areas across 21 months. 27 unique IDs. Zero defects.

---
## What Was Built

Viewport 1100x600. 8 triSolid@1 areas stacked from 0% to 100%.
Each area's height represents one department's share of monthly revenue.

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
- Tools & Hardware consistently holds the largest share.
- Department shares are relatively stable month-to-month, suggesting stable product mix.

---
## Lessons
1. Stacked area charts require careful baseline tracking — each band sits on top of the previous one.
2. Percentage normalization reveals share changes that absolute values would hide.
