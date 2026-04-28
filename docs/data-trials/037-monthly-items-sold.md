# Data Trial 037: Monthly Items Sold
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** Custom aggregation — sale_items quantity grouped by sale month
**Goal:** Bar chart of total items sold per month.
**Outcome:** Volume trends visible across 21 months. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. 21 bars for 21 months.
Range: 1559 to 3251 units/month.
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
- Monthly unit sales range from 1559 to 3251.
- Volume trends may differ from revenue trends due to product mix changes.
---
## Lessons
1. Joining sale_items → sales via saleId → date gives per-month item counts not available in the adapter.
