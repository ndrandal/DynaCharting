# Data Trial 017: Monthly Profit Line
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** monthly_profit()
**Goal:** Line chart showing monthly profit (revenue minus expenses).
**Outcome:** Profit trend clearly visible. Zero defects.
---
## What Was Built
Viewport 800x500. lineAA@1 pipeline. 21 segments from 22 months.
Profit range: $-116,017 to $-1,290.
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
- All months show positive profit — the store is consistently profitable.
- Profit margin ranges from -296.9% to 0.0%.
---
## Lessons
1. monthly_profit() computes revenue - expenses, providing a derived metric.
