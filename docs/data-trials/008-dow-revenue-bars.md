# Data Trial 008: Day-of-Week Revenue Bars
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** dow_distribution()
**Goal:** 7 bars showing revenue by day of week (Mon-Sun).
**Outcome:** Weekly pattern visible. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. 7 bars for 7 days.
Peak day: Sat ($281,441).
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
- Sat is the highest revenue day at $281,441.
- Weekend days show distinct patterns compared to weekdays.
---
## Lessons
1. dow_distribution() uses 0=Mon..6=Sun matching Python's weekday() convention.
