# Data Trial 012: Customer Tier Revenue Bars
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** customer_tier_revenue()
**Goal:** Bar chart of revenue by customer tier with tier-specific colors.
**Outcome:** Three colored bars clearly show tier contribution. Zero defects.
---
## What Was Built
Viewport 600x500. instancedRect@1 pipeline. 3 bars (gold/silver/bronze).
Revenue: gold=$142,061, silver=$173,569, bronze=$214,281.
Total: 12 unique IDs.
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
- Bronze tier generates the most revenue.
- Average spend: gold=$107, silver=$110, bronze=$107.
---
## Lessons
1. Three separate DrawItems needed for three different bar colors in a single chart.
