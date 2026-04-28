# Data Trial 020: Items Per Sale Histogram
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** items_per_sale_distribution()
**Goal:** Histogram of how many items per transaction.
**Outcome:** Clear distribution. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. 8 bars for 8 item counts.
Peak: 1 items per sale (3523 occurrences).
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
- Most transactions contain 1 item(s) (3523 sales).
- Distribution is right-skewed — few transactions have many items.
---
## Lessons
1. items_per_sale_distribution uses itemCount as both the x position and the category — natural for histograms.
