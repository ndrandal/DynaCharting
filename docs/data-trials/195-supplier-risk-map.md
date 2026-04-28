# Data Trial 195: Supplier Risk Map
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Risk assessment scatter with colored quadrant backgrounds. High concentration + high lead time = highest risk. Requires background zone fills behind data points.
**Goal:** 12 suppliers on risk scatter: X=cost concentration, Y=lead time. Color-coded quadrants.
**Outcome:** 12 suppliers plotted. Median concentration: 8.1%, median lead: 6.9 days. 17 unique IDs. Zero defects.

---
## What Was Built

Viewport 900x700. Three layers: tinted quadrant backgrounds, dashed dividers, orange data points.
Red-tinted top-right = high risk. Green-tinted bottom-left = low risk.

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
- Suppliers with both high cost concentration and long lead times are supply chain risks.
- Most suppliers cluster in the middle, with few extreme-risk cases.

---
## Lessons
1. Background zone fills (tinted instancedRect@1 at low alpha) provide risk context without obscuring data.
2. Layer ordering: zones behind grid lines behind data points.
