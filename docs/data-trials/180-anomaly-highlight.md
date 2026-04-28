# Data Trial 180: Anomaly Highlight
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Statistical anomaly detection — flag days where revenue exceeds 2 standard deviations above the 30-day rolling mean. Red points overlay the revenue line.
**Goal:** Daily revenue line + red point markers on anomalous high-revenue days.
**Outcome:** 37 anomalies detected out of 630 days. 10 unique IDs. Zero defects.

---
## What Was Built

Viewport 1200x500. Gray lineAA@1 for daily revenue + red points@1 on anomaly days.
Statistical computation: rolling 30-day window, flag values > mean + 2*std.

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
- 37 anomalous days detected (~5.9% of all days).
- Anomalies likely correspond to holiday sales events, promotions, or large bulk orders.

---
## Lessons
1. Statistical analysis in Python + visual flagging in the engine creates effective anomaly detection views.
2. points@1 overlay on lineAA@1 is a natural pattern for highlighting specific data points.
