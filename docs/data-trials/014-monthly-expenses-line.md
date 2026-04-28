# Data Trial 014: Monthly Expenses Line
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** monthly_expenses()
**Goal:** Line chart showing monthly expense totals over 21 months.
**Outcome:** Clean expense trend line. Zero defects.
---
## What Was Built
Viewport 800x500. lineAA@1 pipeline. 21 segments from 22 monthly points.
Expense range: $1,290 to $158,056.
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
- Monthly expenses range from $1,290 to $158,056.
- Expenses show relatively stable patterns compared to revenue volatility.
---
## Lessons
1. Same line pipeline works for any time-series data — revenue or expenses.
