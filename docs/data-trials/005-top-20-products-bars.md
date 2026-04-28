# Data Trial 005: Top 20 Products Horizontal Bars
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** product_rankings(20)
**Goal:** Horizontal bar chart of top 20 products by revenue.
**Outcome:** Clear ranking visualization. Zero defects.
---
## What Was Built
Viewport 900x600. instancedRect@1 pipeline. 20 horizontal bars for top 20 products sorted ascending (highest at top).
Top product: Cordless Drill/Driver 20V Kit ($77,152).
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
- Top product revenue is $77,152, a significant lead over #2 at $34,146.
- The top 20 products represent a large share of total store revenue.
---
## Lessons
1. to_horizontal_bars(items, y_key, x_key) — note parameter order: y_key is category, x_key is value.
