# Data Trial 196: Capacity Gap Analysis
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Overlay staff availability (from 13,751 shifts) with sales demand (from 12,338 sales) per hour. Reveals staffing gaps.
**Goal:** Paired bars per hour: average staff count vs average daily sales count.
**Outcome:** 14 hour pairs. Normalized to per-day averages. 9 unique IDs. Zero defects.

---
## What Was Built

Viewport 1100x600. Side-by-side bars per hour: blue (staff) left, orange (sales) right.
Both metrics normalized to daily averages for comparability.

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
- Hours where orange exceeds blue indicate understaffing (more sales than staff).
- Early morning and late evening typically have excess staff capacity.

---
## Lessons
1. Paired bars (offset half-widths) provide direct visual comparison of two metrics.
2. Normalizing to the same time unit (per-day) is critical for meaningful comparison.
