# Data Trial 028: Revenue Growth % Month-over-Month
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** Derived from monthly_revenue() — (curr-prev)/prev*100
**Goal:** Bar chart of month-over-month revenue growth percentage (green positive, red negative).
**Outcome:** Growth/decline pattern visible. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. 20 bars (12 positive, 8 negative).
Growth range: -54.5% to 107.5%.
Total: 9 unique IDs.
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
- 12 months show growth, 8 months show decline.
- Growth ranges from -54.5% to 107.5%.
---
## Lessons
1. Diverging bar charts (positive/negative) require two DrawItems with different colors.
