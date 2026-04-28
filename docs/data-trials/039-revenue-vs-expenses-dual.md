# Data Trial 039: Revenue vs Expenses Dual Line
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** monthly_revenue() + monthly_expenses()
**Goal:** Overlay revenue (blue) and expenses (red) lines on the same chart.
**Outcome:** Clear visual comparison of revenue vs expenses. Zero defects.
---
## What Was Built
Viewport 800x500. Two lineAA@1 DrawItems sharing one transform.
Revenue: 20 segments (blue). Expenses: 20 segments (red).
Shared Y-axis range to show both series at correct relative scale.
Total: 9 unique IDs.
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
- Revenue consistently exceeds expenses — the store is profitable.
- The gap between lines represents monthly profit.
---
## Lessons
1. Dual-line charts share one transform but need separate buffer/geometry/drawItem triples.
