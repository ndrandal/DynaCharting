# Trial 081: Revenue by Product Category

**Date:** 2026-03-22
**Goal:** Join sale_items to products, aggregate revenue by category, show top 15 as vertical bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON sale_items.productId = products.id
GROUP BY products.category
SUM(sale_items.lineTotal) AS revenue
ORDER BY revenue DESC
LIMIT 15
```

Join: sale_items (33,834 rows) x products (150 rows) via productId.
Aggregation: sum lineTotal per category, sort descending, take top 15.

## Data Insight

| Category | Revenue |
|----------|---------|
| power tools | $177,626 |
| hand tools | $58,005 |
| lighting | $44,919 |
| wire | $43,583 |
| fixtures | $42,681 |
| safety | $41,226 |
| power equip | $37,630 |
| interior paint | $37,456 |
| heating | $31,339 |
| power | $30,470 |
| measuring | $29,274 |
| sheathing | $27,138 |
| cords | $26,128 |
| weatherproof | $22,764 |
| fans | $21,682 |

The top category alone accounts for a significant share of total revenue. Hardware staples
(fasteners, hand tools, pipe, fittings) dominate, reflecting the store's identity as a
hardware-first retailer.

---

## What Was Built

1000x500 viewport, single pane, 15 blue instancedRect bars with corner radius.
Transform maps category indices 0-14 on X and revenue on Y to clip space.

Total: 6 unique IDs.
