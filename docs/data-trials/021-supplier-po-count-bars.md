# Data Trial 021: Supplier PO Count Bars
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** supplier_performance()
**Goal:** Bar chart of purchase order count per supplier.
**Outcome:** Supplier activity visible. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. 12 bars for 12 suppliers.
Most active: GreenGrow Distributors (54 POs).
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
- GreenGrow Distributors leads with 54 purchase orders.
- 12 suppliers service the store.
---
## Lessons
1. supplier_performance() provides poCount, avgLeadTime, and totalCost — multiple visualization angles.
