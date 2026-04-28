# Data Trial 019: Average Ticket Trend
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** monthly_revenue() — avgTicket field
**Goal:** Line chart of average transaction value per month.
**Outcome:** Ticket trend visible. Zero defects.
---
## What Was Built
Viewport 800x500. lineAA@1 pipeline. 20 segments from 21 months.
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
- Average ticket varies from $91.87 to $118.69.
- Relatively stable, suggesting consistent basket composition.
---
## Lessons
1. avgTicket = revenue / count — a derived field already computed by the adapter.
