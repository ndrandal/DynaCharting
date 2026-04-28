# Data Trial 199: Database Summary Viz
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Meta-visualization — the database visualizing itself. Each table becomes a horizontal bar sized by record count.
**Goal:** 15 database tables as horizontal bars showing their record counts.
**Outcome:** 15 horizontal bars. Largest: sale_items (33,834). Total: 65,097 records. 6 unique IDs. Zero defects.

---
## What Was Built

Viewport 900x600. Horizontal instancedRect@1 bars sorted by size.
Purple color scheme for a "database/technical" aesthetic.
Labels show table name and record count.

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
- sale_items dominates with 33,834 records — the most granular table.
- The top 3 tables (sale_items, shifts, sales) contain ~92% of all records.
- Reference tables (departments, zones, store) are tiny by comparison.

---
## Lessons
1. Meta-visualization reveals data structure at a glance — useful for understanding any database.
2. Log scale would be better here (range from 1 to 33K) but linear still communicates the dominance.
