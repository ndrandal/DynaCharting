# Trial 113: Product Lifecycle — Top 3

**Date:** 2026-03-22
**Goal:** Monthly revenue for top 3 products over 21 months. 3 overlaid lines.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
product_rankings() → top 3 products: Cordless Drill/Driver 20V Kit, Circular Saw 7-1/4in 15A, Romex 14/2 NM-B 250ft
sale_items WHERE productId IN top3
  JOIN sales (via _sale_month) → month
GROUP BY (productId, month) → SUM(lineTotal)
```

Two-table join with per-product temporal aggregation.

## Data Insight

Each product's revenue line shows its lifecycle trajectory. Some products have steady
demand; others may show seasonal peaks or gradual growth/decline.

---

## What Was Built

1200x500 viewport, 3 colored lineAA@1 lines sharing one transform.

Total: 12 unique IDs.
