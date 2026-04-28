# Data Trial 030: Supplier Lead Time Bars
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** supplier_performance()
**Goal:** Bar chart of average lead time per supplier, sorted ascending.
**Outcome:** Supplier speed ranking clear. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. 12 bars for 12 suppliers.
Fastest: GreenGrow Distributors (4.0 days). Slowest: HomeStyle Imports (10.5 days).
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
- Lead times range from 4.0 to 10.5 days.
- GreenGrow Distributors is the fastest supplier.
---
## Lessons
1. Sorting by the y-axis value before plotting makes comparison intuitive.
