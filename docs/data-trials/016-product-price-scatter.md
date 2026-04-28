# Data Trial 016: Product Price vs Volume Scatter
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** product_price_vs_volume()
**Goal:** Scatter plot of unit price (x) vs units sold (y) for all 150 products.
**Outcome:** Clear price-volume relationship visible. Zero defects.
---
## What Was Built
Viewport 800x500. points@1 pipeline. 150 scatter points.
Price range: $0.79 to $899.99.
Volume range: 34 to 657 units.
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
- General inverse relationship: higher-priced items tend to sell fewer units.
- Some outliers show high price AND high volume — premium bestsellers.
---
## Lessons
1. to_scatter uses pos2_clip format (8B per point) — simplest vertex format.
