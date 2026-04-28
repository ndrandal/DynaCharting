# Trial 143 — Inventory Health Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: stock level overlay lines (top), turnover rates (bottom-left), below-reorder count (bottom-right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `inventory_trend()` for top 5 products — overlaid lineAA@1 lines
- Pane 2: computed turnover rates — 5 bars as instancedRect@1
- Pane 3: below-reorder snapshot count — 5 bars as instancedRect@1

## Insight
Multi-product inventory overlay shows which products are declining fastest. Turnover and below-reorder metrics identify replenishment priorities.
