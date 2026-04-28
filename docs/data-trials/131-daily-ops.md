# Trial 131 — Daily Operations Dashboard
**Date:** 2026-03-22
**Layout:** 4 panes: daily revenue line, hourly sales curve, shift heatmap, items-per-sale histogram
**Resolution:** 1200x800

## Data Sources
- Pane 1: `daily_revenue()` — 630 days as lineAA@1
- Pane 2: `hourly_distribution()` — 14 hours as lineAA@1
- Pane 3: `shift_heatmap()` — 126 cells as banded instancedRect@1
- Pane 4: `items_per_sale_distribution()` — 8 bins as instancedRect@1

## Insight
Operational command center combining revenue trends, peak hours, staffing coverage, and basket size distribution for day-to-day decision making.
