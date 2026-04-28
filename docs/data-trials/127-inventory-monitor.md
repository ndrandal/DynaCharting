# Trial 127 — Inventory Monitor Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: inventory trend (top), reorder frequency (bottom-left), supplier lead times (bottom-right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `inventory_trend(23)` — 21 snapshots as lineAA@1
- Pane 2: reorder frequency — 15 products as instancedRect@1
- Pane 3: `supplier_performance()` — 12 suppliers as instancedRect@1

## Insight
Tracks top-product inventory levels, flags products that frequently hit reorder points, and compares supplier delivery performance for procurement decisions.
