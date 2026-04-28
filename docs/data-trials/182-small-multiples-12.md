# Data Trial 182: Small Multiples 12
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** 12 mini-panes (4x3 grid), each showing daily revenue for one calendar month. Requires 12 panes, 12 layers, 12 transforms with independent Y scales.
**Goal:** Seasonal comparison — same metric across all 12 months in small-multiple layout.
**Outcome:** 12 panes with independent lineAA@1 plots. 72 unique IDs. Zero defects.

---
## What Was Built

Viewport 1200x900. 12 panes arranged 4 columns x 3 rows.
Each pane shows daily revenue for one calendar month with its own Y-axis scale.

Month data aggregated across all years in the dataset (Jul 2024 — Mar 2026).

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
- Each month has its own revenue pattern across the ~31 days.
- Seasonal patterns emerge: some months have higher daily variance than others.

---
## Lessons
1. Small multiples are a natural fit for the multi-pane architecture — each pane is truly independent.
2. 12 panes with independent transforms is straightforward but requires careful ID allocation.
