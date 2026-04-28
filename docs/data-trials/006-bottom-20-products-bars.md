# Data Trial 006: Bottom 20 Products (Slow Movers)
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** product_rankings() — last 20
**Goal:** Identify the 20 slowest-selling products by revenue.
**Outcome:** Clear visualization of underperformers. Zero defects.
---
## What Was Built
Viewport 900x600. instancedRect@1 pipeline. 20 horizontal bars for bottom 20 products (red).
Lowest: R-13 Insulation Batt 15in ($215).
Total: 6 unique IDs.
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
- Bottom 20 products have dramatically lower revenue than top performers.
- These slow movers may be candidates for markdown or discontinuation.
---
## Lessons
1. Slicing product_rankings() gives bottom-N when taking from the end of the sorted list.
