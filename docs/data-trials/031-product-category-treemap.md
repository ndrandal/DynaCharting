# Data Trial 031: Product Category Treemap
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** department_revenue()
**Goal:** Treemap-style rectangles proportional to department revenue.
**Outcome:** Area-proportional department visualization. Zero defects.
---
## What Was Built
Viewport 800x600. instancedRect@1 pipeline. 8 rectangles packed as horizontal strips.
Rectangles sized proportional to revenue share. PALETTE_DEPT colors.
Total: 26 unique IDs.
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
- Tools & Hardware gets the largest rectangle (25.8% of total).
- Simple strip packing sufficient for 8 categories.
---
## Lessons
1. Treemaps in DynaCharting are just instancedRect@1 with computed positions — no special pipeline needed.
