# Data Trial 036: Daily Revenue Sparkline
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** daily_revenue()
**Goal:** Minimal sparkline — 630 days of revenue, tight margins, no grid.
**Outcome:** Dense sparkline suitable for inline display. Zero defects.
---
## What Was Built
Viewport 400x100 (sparkline proportions). lineAA@1 pipeline. 629 segments.
Tight clip margins (0.98), minimal padding (1%). lineWidth 1.0.
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
- 630 data points compressed into a sparkline form factor.
- Overall trend and volatility visible even at small size.
---
## Lessons
1. Sparklines use tight clip margins and small viewports — fit_transform padding can be reduced to 1%.
