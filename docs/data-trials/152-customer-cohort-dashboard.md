# Trial 152 — Customer Cohort Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: cohort revenue bars (left), cumulative growth line (center), tier donut (right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `customer_tier_revenue()` — 3 tiers as instancedRect@1
- Pane 2: customer join dates — 0 months as lineAA@1
- Pane 3: `customer_tier_breakdown()` — 3 tiers as donut

## Insight
Revenue by tier shows tier value; growth curve shows customer acquisition pace; donut shows current composition.
