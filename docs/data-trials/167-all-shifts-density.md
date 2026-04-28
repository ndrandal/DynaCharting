# Data Trial 167: All Shifts Density
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Heatmap from 13,751 shifts binned into 357 cells (17 hours x 21 months). Per-cell coloring requires creative bucketing since instancedRect@1 uses one color per DrawItem.
**Goal:** Hour-of-day x month density heatmap showing staffing patterns.
**Outcome:** 357 cells in 8 color buckets. Peak density: 615 shifts/cell. 27 unique IDs. Zero defects.

---
## What Was Built

Viewport 1200x700. Heatmap grid with 17 rows (hours 6-22) x 21 columns (months).
Color buckets from black (low) through yellow to white (high).

Creative solution for per-cell coloring: quantize values into 8 buckets, each bucket gets its own DrawItem with a representative color. This approximates a continuous heatmap.

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
- Peak staffing hours: 9am-5pm across all months.
- Evening hours (after 7pm) have minimal shift coverage.
- Consistent staffing pattern across months with slight seasonal variation.

---
## Lessons
1. Heatmaps with per-cell color require creative solutions in a single-color-per-DrawItem engine — color bucketing is an effective approximation.
2. 8 color levels provide sufficient visual resolution for most heatmap use cases.
