# Data Trial 173: Negative Values Profit
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Diverging bars crossing zero — some months are profitable, others are not. Requires separate DrawItems for positive (green) and negative (red) bars.
**Goal:** Monthly profit (revenue minus ALL expenses) as diverging bars from zero baseline.
**Outcome:** 0 positive months (green), 22 negative months (red). Range: $-116,017 to $-1,290. 10 unique IDs. Zero defects.

---
## What Was Built

Viewport 1100x600. Two DrawItems: green bars above zero, red bars below zero.
Dashed zero baseline line. Transform maps the full profit range to clip space.

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
- Most months show losses — expenses exceed revenue significantly.
- This is typical for a hardware store where large periodic costs (rent, payroll) dominate.
- Profit range: $-116,017 to $-1,290.

---
## Lessons
1. Diverging bar charts require separate DrawItems for positive and negative values to get distinct colors.
2. A dashed zero baseline provides critical context for interpreting profit/loss charts.
