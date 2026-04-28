# Data Trial 007: Hourly Sales Histogram
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** hourly_distribution()
**Goal:** Bar chart showing transaction count by hour of day.
**Outcome:** Clear hourly distribution. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. 14 bars for 14 active hours.
Peak hour: 14:00 with 1603 transactions.
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
- Peak sales around hour 14:00 (1603 transactions).
- Store hours visible from the distribution — no sales outside operating hours.
---
## Lessons
1. hourly_distribution() only returns hours with sales > 0, leaving gaps for closed hours.
