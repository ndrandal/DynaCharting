# Data Trial 022: Inventory Trend (Top Product)
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** inventory_trend(product_id) for top product
**Goal:** Line chart of on-hand inventory over time for the highest-revenue product.
**Outcome:** Stock level fluctuations visible. Zero defects.
---
## What Was Built
Viewport 800x500. lineAA@1 pipeline. 20 segments from 21 snapshots.
Product: Cordless Drill/Driver 20V Kit.
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
- Inventory quantity ranges from 41 to 196 units.
- Periodic restocking patterns visible in the sawtooth shape.
---
## Lessons
1. inventory_trend(product_id) requires a valid product ID — may return empty if product has no snapshots.
