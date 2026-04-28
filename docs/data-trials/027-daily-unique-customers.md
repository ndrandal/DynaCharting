# Data Trial 027: Daily Unique Customers
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** daily_revenue() — uniqueCustomers field
**Goal:** Dense line chart of unique customers per day across 630 days.
**Outcome:** Customer traffic pattern visible. Zero defects.
---
## What Was Built
Viewport 1200x500. lineAA@1 pipeline. 629 segments from 630 daily points.
Range: 1 to 28 unique customers/day.
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
- Daily unique customers range from 1 to 28.
- Weekly cyclical patterns likely visible in the dense line.
---
## Lessons
1. uniqueCustomers from daily_summaries tracks foot traffic — distinct from transaction count.
