# Data Trial 034: Product Cost vs Price Scatter
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** Custom from db.products — unitCost vs unitPrice
**Goal:** Scatter plot of product unitCost (x) vs unit price (y).
**Outcome:** Relationship between unitCost and price visible. Zero defects.
---
## What Was Built
Viewport 800x500. points@1 pipeline. 150 scatter points.
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
- 150 products plotted.
- Shows correlation (or lack thereof) between unitCost and retail price.
---
## Lessons
1. Not all product fields may be populated — fallback strategies keep the trial functional.
