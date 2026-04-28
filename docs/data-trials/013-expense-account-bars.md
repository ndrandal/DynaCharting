# Data Trial 013: Expense by Account Horizontal Bars
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** expense_by_account()
**Goal:** Horizontal bar chart of 13 expense accounts sorted by total.
**Outcome:** Clear expense ranking. Zero defects.
---
## What Was Built
Viewport 900x600. instancedRect@1 pipeline. 13 horizontal bars for 13 accounts.
Top expense: Salaries & Wages ($2,002,024).
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
- Salaries & Wages is the largest expense at $2,002,024.
- 13 expense accounts span types: Facilities, Financial, Loss, Occupancy, OpEx, Payroll.
---
## Lessons
1. Horizontal bars work well for long category labels — 13 accounts fit cleanly.
