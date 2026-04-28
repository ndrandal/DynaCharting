# Data Trial 033: Customer Join Histogram by Quarter
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** Custom aggregation from db.customers — memberSince dates grouped by quarter
**Goal:** Histogram of customer join dates by quarter.
**Outcome:** Customer acquisition pattern visible. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. 24 bars for 24 quarters.
Quarters span: 2020Q2 to 2026Q1.
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
- Customer acquisition varies by quarter.
- 24 quarters represented with 500 total customers.
---
## Lessons
1. Raw customer data needs manual date parsing and grouping — the adapter does not provide a built-in query.
