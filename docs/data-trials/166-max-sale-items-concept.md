# Data Trial 166: Max Sale Items Concept
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Representing 33,834 records visually. Full dataset would produce 270K floats — sampled 2,000 for feasibility.
**Goal:** Concept visualization: each sale item as a colored point (saleId vs lineTotal).
**Outcome:** 2,000 points from random sample. Line total range: $0.70–$1,009.52. 6 unique IDs. Zero defects.

---
## What Was Built

Viewport 1200x600. 2,000 sampled sale items as points@1 with low alpha (0.4) and small size (3px).
Reveals the distribution of line totals across the entire sales history.

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
- Most line totals cluster below $200 — consistent with a hardware store.
- Outliers reach $1,009.52 (likely large lumber or tool purchases).
- Uniform ID spacing confirms even sampling across the date range.

---
## Lessons
1. Sampling is a practical solution for datasets that exceed reasonable vertex counts.
2. Low alpha (0.4) + small point size (3px) makes 2,000 overlapping points readable.
