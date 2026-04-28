# Data Trial 164: 630-Day Dense Line
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** 630 data points as a continuous line — tests dense lineAA@1 rendering with 629 segments.
**Goal:** All 630 daily revenue values as a single lineAA@1 line.
**Outcome:** 629 line segments spanning 2024-07-01 to 2026-03-22. Revenue range: $431–$8,816. 6 unique IDs. Zero defects.

---
## What Was Built

Viewport 1200x500. Single dense lineAA@1 with 629 segments.
Interactive viewport for pan/zoom to explore the ~21 months of data.

At full zoom-out, each pixel column represents ~0.5 days. The line width of 1.5px is thin enough to show daily volatility.

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
- Revenue range: $431 to $8,816 per day.
- Clear weekly periodicity visible: weekday highs, weekend dips.

---
## Lessons
1. 629 lineAA@1 segments render without issue — the engine handles dense data well.
2. Thin line width (1.5px) is important for dense data to prevent visual mudding.
