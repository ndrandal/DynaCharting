# Data Trial 009: Payment Method Pie Chart
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** payment_method_breakdown()
**Goal:** Pie chart showing revenue share by payment method.
**Outcome:** Clean pie with 4 wedges. Zero defects.
---
## What Was Built
Viewport 600x600. triSolid@1 pipeline. 4 wedges, 105 total vertices.
Methods: credit_card (51.1%), debit_card (24.3%), cash (21.4%), check (3.1%).
Total: 14 unique IDs.
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
- credit_card dominates at 51.1% of revenue.
- 4 payment methods in use.
---
## Lessons
1. to_pie_wedges returns one (data, fraction, startAngle, endAngle) tuple per wedge — each needs its own buffer/geometry/drawItem.
