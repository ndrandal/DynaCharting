# Data Trial 168: Hierarchical Treemap
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Two-level treemap layout — 8 departments subdivided by top 5 categories each. Requires computing nested area-proportional rectangles.
**Goal:** Nested treemap showing revenue hierarchy: departments (outer) and product categories (inner).
**Outcome:** 8 department boxes with ~40 category cells. 126 unique IDs. Zero defects.

---
## What Was Built

Viewport 1100x700. Two-row layout (4 departments per row).
Each department box subdivided horizontally by its top 5 product categories, sized proportionally by revenue.

Department color from PALETTE_DEPT with varying alpha per category (darker = more revenue).
lineAA@1 outlines separate departments.

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
- Tools & Hardware dominates the treemap with the largest box.
- Category subdivision reveals which product lines drive each department.

---
## Lessons
1. Treemaps can be approximated with instancedRect@1 by computing rectangle positions in Python.
2. Each cell needs its own DrawItem for individual coloring — this means many IDs for detailed treemaps.
