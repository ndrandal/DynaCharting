# Data Trial 165: 500 Customer Scatter
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** 500 overlapping points — tests point rendering density and alpha blending for overplotting.
**Goal:** All 500 customers plotted: X = days since membership, Y = total spend. Shows loyalty vs value relationship.
**Outcome:** 500 points@1 with alpha=0.6 for overplot handling. Spend range: $3–$6,020. 6 unique IDs. Zero defects.

---
## What Was Built

Viewport 900x700. points@1 scatter with 500 vertices.
Alpha=0.6 to handle overlapping points — denser regions appear brighter.
Point size 5px keeps individual points distinguishable.

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
- Average customer spend: $1,060
- Newer customers (short membership) tend to have lower total spend — expected correlation.
- A few long-term customers with very high spend are visible as outliers.

---
## Lessons
1. Alpha transparency is essential for dense scatter plots — 500 points with full opacity would be an opaque blob.
2. points@1 handles 500 instances efficiently.
