# Trial 099: Inventory Turnover (Top 20)

**Date:** 2026-03-22
**Goal:** For each product: total units sold / average inventory. Top 20 by turnover.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items GROUP BY productId → SUM(quantity) AS unitsSold
inventory_snapshots GROUP BY productId → AVG(quantityOnHand) AS avgInventory
turnover = unitsSold / avgInventory
ORDER BY turnover DESC LIMIT 20
```

Two-table cross-reference: sale_items (33,834) x inventory_snapshots (3,150).

## Data Insight (Top 10)

| Product | Units Sold | Avg Inv | Turnover |
|---------|-----------|---------|----------|
| Supply Line Braided 3/8x2 | 485 | 79 | 6.1x |
| Painter's Tape 1.88in x 6 | 585 | 99 | 5.9x |
| 2x4x8 Kiln-Dried Stud | 657 | 112 | 5.9x |
| Shutoff Valve 1/2in | 399 | 69 | 5.8x |
| Battery Pack AA 24-Count | 547 | 96 | 5.7x |
| Drywall Sheet 4x8 1/2in | 441 | 78 | 5.6x |
| Garden Hose 5/8in x 50ft | 601 | 107 | 5.6x |
| Concrete Mix 80lb Bag | 604 | 107 | 5.6x |
| Leaf Rake 24in | 469 | 83 | 5.6x |
| P-Trap 1-1/2in PVC | 491 | 88 | 5.6x |

High-turnover products move quickly relative to stock levels — they are the store's
workhorses. Low turnover suggests overstock or slow-moving inventory.

---

## What Was Built

1000x500 viewport, 20 cyan bars showing inventory turnover ratio.

Total: 6 unique IDs.
