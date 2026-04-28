# Trial 134 — Customer Segmentation Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: tier pie (left), tier revenue bars (center), customer frequency scatter (right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `customer_tier_breakdown()` — 3 tiers as pie (triSolid@1)
- Pane 2: `customer_tier_revenue()` — 3 tiers as instancedRect@1
- Pane 3: customer purchase counts — 500 customers as points@1

## Insight
Scatter plot reveals the long tail: most customers make few purchases, while a handful are heavy buyers. Combined with tier distribution, this guides loyalty program targeting.
