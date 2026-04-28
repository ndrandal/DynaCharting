# Data Trial 025: Product Margin vs Revenue Scatter
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** product_rankings(150)
**Goal:** Scatter plot of profit margin (x) vs revenue (y) for all products.
**Outcome:** Margin-revenue distribution visible. Zero defects.
---
## What Was Built
Viewport 800x500. points@1 pipeline. 150 scatter points.
Margin range: 0.308 to 0.597.
Revenue range: $215 to $77,152.
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
- High-margin products are not necessarily the highest revenue generators.
- Some low-margin products drive significant revenue through volume.
---
## Lessons
1. Scatter plots reveal relationships between two continuous variables that bar/line charts cannot.
