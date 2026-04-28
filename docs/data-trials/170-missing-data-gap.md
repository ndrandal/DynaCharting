# Data Trial 170: Missing Data Gap
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Representing missing data — days 200-220 removed to simulate a data outage. The line must NOT connect across the gap.
**Goal:** Daily revenue line with a visible 21-day gap. Two separate line segments with gap markers.
**Outcome:** Two lineAA@1 segments (199 + 408 segments) with red dashed gap markers. 13 unique IDs. Zero defects.

---
## What Was Built

Viewport 1200x500. Two separate lineAA@1 DrawItems (before/after gap) sharing the same transform.
Red dashed vertical lines mark the gap boundaries at day 200 and day 220.

Key technique: splitting the data into two DrawItems prevents the engine from drawing a misleading line across the missing period.

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
- The gap represents 21 days of missing data — ~3.3% of the 630-day series.
- The line resumes at a similar level after the gap, suggesting no structural change during the outage.

---
## Lessons
1. Missing data requires separate DrawItems — a single line would interpolate across the gap, which is misleading.
2. Visual markers (dashed lines) communicate data quality issues explicitly.
