# Trial 087: Top Seller Product Mix

**Date:** 2026-03-22
**Goal:** Find #1 employee by sales count, then show their top 15 products sold. Bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
Step 1: sales GROUP BY employeeId → top employee = Carlos Mendoza (emp #18)
Step 2: sales WHERE employeeId=18 → sale_ids
Step 3: sale_items WHERE saleId IN sale_ids
        JOIN products ON productId
        GROUP BY productId, SUM(quantity)
        ORDER BY units DESC LIMIT 15
```

Three-table chain: employees -> sales -> sale_items -> products.

## Data Insight

| Product | Units Sold |
|---------|-----------|
| Light Switch Single- | 46 |
| PVC Pipe 1-1/2in x 1 | 37 |
| Garden Hose 5/8in x  | 36 |
| 2x4x8 Kiln-Dried Stu | 35 |
| Wire Nuts Assorted 1 | 34 |
| Paint Roller Cover 9 | 32 |
| Round-Point Shovel 4 | 32 |
| Claw Hammer 16oz | 32 |
| Mulch Bag 2 cu-ft Re | 31 |
| Utility Knife Retrac | 29 |
| Planter Pot Ceramic  | 28 |
| Electrical Box Singl | 28 |
| Tape Measure 25ft | 27 |
| Wood Stain Interior  | 26 |
| Safety Glasses Clear | 26 |

Carlos Mendoza's product mix reveals whether they are a generalist (selling across departments)
or a specialist focused on one category.

---

## What Was Built

1000x500 viewport, 15 violet bars for the top seller's product breakdown.

Total: 6 unique IDs.
