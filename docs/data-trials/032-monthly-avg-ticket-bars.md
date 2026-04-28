# Data Trial 032: Monthly Average Ticket Bars
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** monthly_revenue() — avgTicket field
**Goal:** Bar chart of average transaction value per month.
**Outcome:** Monthly ticket values clearly comparable. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. 21 bars for 21 months.
Avg ticket range: $91.87 to $118.69.
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
- Average ticket is relatively stable month-to-month.
- Range: $91.87 to $118.69.
---
## Lessons
1. Bar chart better shows month-to-month comparison of average ticket than a line.
