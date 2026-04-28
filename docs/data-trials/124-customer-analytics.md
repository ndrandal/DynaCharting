# Trial 124 — Customer Analytics Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: tier donut (left), tier revenue bars (center), avg spend bars (right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `customer_tier_breakdown()` — 3 tiers as donut (triSolid@1)
- Pane 2: `customer_tier_revenue()` — 3 tiers as instancedRect@1
- Pane 3: `customer_tier_revenue()` — avgSpend field as instancedRect@1

## Insight
Gold tier is smallest in count but highest in revenue per customer. Silver tier drives the most total revenue by volume.
