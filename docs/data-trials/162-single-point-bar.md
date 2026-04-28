# Data Trial 162: Single Point Bar
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Minimal data scenario — a single bar for one month. Tests transform computation with a trivially narrow x-range.
**Goal:** Show only March 2026 revenue as a single bar.
**Outcome:** 1 bar, $63,738.85 revenue from 537 transactions. 6 unique IDs. Zero defects.

---
## What Was Built

Viewport 600x400. Single pane, single bar. instancedRect@1 with rounded corners.
Transform maps the single bar at x=0 to fill the center of clip space.

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
- March 2026 (partial month): $63,738.85 from 537 transactions.

---
## Lessons
1. Single-bar charts need careful transform: x-range is degenerate (single point), so padding is critical.
2. Even with one data point, the chart communicates useful information when properly labeled.
