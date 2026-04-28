# Data Trial 002: Daily Revenue Line
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** daily_revenue()
**Goal:** Plot all 630 days of daily revenue as a dense line chart.
**Outcome:** Dense but readable line. Zero defects.
---
## What Was Built
Viewport 1200x500. lineAA@1 pipeline with rect4 format. 629 line segments from 630 daily points.
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
- Daily revenue shows high variance with visible weekly patterns (lower weekday dips).
- ~630 business days span the full 21-month period.
---
## Lessons
1. lineAA@1 handles 629 segments cleanly at lineWidth 1.0 — no performance concern.
