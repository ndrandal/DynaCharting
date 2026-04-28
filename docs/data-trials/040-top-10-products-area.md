# Data Trial 040: Top 10 Products Stacked Area
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** Derived from sale_items + product_rankings(10) — per-product monthly revenue
**Goal:** Stacked area chart showing top 10 products' monthly revenue contribution.
**Outcome:** Stacked areas show product contribution over time. Zero defects.
---
## What Was Built
Viewport 1000x600. triSolid@1 pipeline. 10 stacked area bands across 21 months.
1200 total vertices. PALETTE_8 colors.
Top product: Cordless Drill/Driver 20V Kit.
Total: 33 unique IDs.
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
- Top 10 products show their relative contribution over 21 months.
- Stacking reveals how total top-product revenue composes.
---
## Lessons
1. Stacked areas require cumulative baselines — each series sits atop the previous.
2. Manual stacking with triSolid@1 (2 triangles per segment) is more work but fully flexible.
