# Trial 089: Inventory vs Sales Rate

**Date:** 2026-03-22
**Goal:** For each product: latest inventory qty vs monthly avg sales rate. Scatter.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
inventory_snapshots: latest snapshot per productId (MAX date)
sale_items JOIN sales → month → GROUP BY (productId, month) → SUM(quantity)
avg_monthly_rate = total_units / months_active
```

Two-table cross-reference: inventory_snapshots (3,150) and sale_items (33,834).

## Data Insight

150 products plotted. Points in the upper-left (low stock, high sales) are
stockout risks. Points in the lower-right (high stock, low sales) are overstock candidates.

This scatter is the foundation of inventory optimization — it reveals which products
need reordering attention vs which are tying up capital.

---

## What Was Built

800x600 viewport, 150 red scatter points. X = current inventory, Y = avg monthly sales rate.

Total: 6 unique IDs.
