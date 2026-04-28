# Data Trial 185: Repeat vs New Revenue
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Classify each sale's revenue as "new customer" (first purchase month) or "repeat" (subsequent months). Stacked bars per month.
**Goal:** Monthly breakdown: repeat customer revenue vs new customer revenue.
**Outcome:** 21 stacked bars. 9 unique IDs. Zero defects.

---
## What Was Built

Viewport 1100x600. Stacked instancedRect@1: blue (repeat) on bottom, green (new) on top.
Customer classification based on their first purchase month.

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
- Repeat customers dominate revenue — new customer acquisition adds a smaller layer.
- Early months show more "new" revenue as the customer base builds.

---
## Lessons
1. Customer cohort analysis requires date-sorted first-purchase detection.
2. Stacked bars are two separate instancedRect@1 DrawItems with offset baselines.
