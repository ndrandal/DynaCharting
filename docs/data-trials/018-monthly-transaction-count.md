# Data Trial 018: Monthly Transaction Count
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** monthly_revenue() — count field
**Goal:** Line chart of transaction count per month.
**Outcome:** Transaction volume trend clear. Zero defects.
---
## What Was Built
Viewport 800x500. lineAA@1 pipeline. 20 segments from 21 months.
Count range: 379 to 767 transactions/month.
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
- Transaction count ranges from 379 to 767 per month.
- Count trends may diverge from revenue if average ticket size changes.
---
## Lessons
1. Same query (monthly_revenue) serves multiple trials by varying the y-axis field.
