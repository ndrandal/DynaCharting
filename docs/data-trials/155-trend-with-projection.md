# Trial 155 — Trend with Projection Dashboard
**Date:** 2026-03-22
**Layout:** 2 panes: top = actual revenue line, bottom = actual + 3-month linear projection (dashed)
**Resolution:** 1200x800

## Data Sources
- Top: `monthly_revenue()` — 21 months as lineAA@1
- Bottom: same + 3 projected months (slope=258.7/month) as dashed lineAA@1

## Insight
Linear regression projects forward. The dashed line shows where revenue is headed if current trends continue. Both panes share the same transform for direct comparison.
