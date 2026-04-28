# Trial 102: Product Co-occurrence

**Date:** 2026-03-22
**Goal:** Find top 10 most common product pairs bought in the same sale. Horizontal bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items GROUP BY saleId → set of productIds per sale
For each sale with 2+ products, enumerate all pairs
COUNT occurrences of each (productA, productB) pair
ORDER BY count DESC LIMIT 10
```

Self-join pattern: sale_items x sale_items on saleId, deduplicated pairs.

## Data Insight

| Product Pair | Co-occurrences |
|-------------|----------------|
| 2x4x8 Kiln-Drie + Painter's Tape  | 20 |
| Paint Brush 2in + Painter's Tape  | 20 |
| 2x4x8 Kiln-Drie + Mulch Bag 2 cu- | 20 |
| Paint Roller Co + Paint Tray Line | 19 |
| Paint Brush 2in + Silicone Caulk  | 19 |
| Tape Measure 25 + Paint Brush 2in | 19 |
| 2x6x8 #2 Lumber + Paint Roller Co | 18 |
| 2x4x8 Kiln-Drie + Garden Gloves N | 18 |
| Teflon Tape 1/2 + Painter's Tape  | 18 |
| Outlet Receptac + Wire Nuts Assor | 18 |

Frequently co-purchased products are cross-sell opportunities. Placing them near each
other on the floor or bundling them in promotions can increase basket size.

---

## What Was Built

1000x500 viewport, 10 blue horizontal bars for top co-occurring product pairs.

Total: 6 unique IDs.
