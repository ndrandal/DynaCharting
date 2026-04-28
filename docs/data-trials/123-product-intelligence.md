# Trial 123 — Product Intelligence Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: top 20 revenue bars (top), price-vs-volume scatter (bottom-left), margin histogram (bottom-right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `product_rankings(top_n=20)` — 20 products as horizontal instancedRect@1
- Pane 2: `product_price_vs_volume()` — 150 products as points@1
- Pane 3: margin distribution — 10 bins as instancedRect@1

## Insight
Identifies top revenue-generating products, reveals the price-volume relationship (expensive items sell fewer units), and shows margin distribution across the catalog.
