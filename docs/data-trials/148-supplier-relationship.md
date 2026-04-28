# Trial 148 — Supplier Relationship Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: cost bars (left), reliability scatter (center), product count bars (right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `supplier_performance()` — 12 suppliers, totalCost as instancedRect@1
- Pane 2: PO count vs lead time — 12 suppliers as points@1
- Pane 3: product count per supplier — 12 suppliers as instancedRect@1

## Insight
Three-dimensional supplier view: cost exposure, reliability (ideal = top-left in scatter: many POs, fast delivery), and product dependency.
