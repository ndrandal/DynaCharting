# Trial 139 — Cash Flow Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes stacked: revenue area (top), expense area (middle), net profit bars (bottom)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `monthly_revenue()` — 21 months as filled area (triSolid@1)
- Pane 2: `monthly_expenses()` — 22 months as filled area (triSolid@1)
- Pane 3: `monthly_profit()` — 22 months as instancedRect@1

## Insight
Cash flow visualization: green area (in) minus red area (out) equals blue bars (net). The stacked layout makes the visual subtraction intuitive.
