# Trial 091: PO Cost vs Product Revenue by Supplier

**Date:** 2026-03-22
**Goal:** By supplier: total PO cost vs total product revenue. Scatter.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
purchase_orders GROUP BY supplierId → SUM(totalCost)
sale_items JOIN products ON productId → GROUP BY supplierId → SUM(lineTotal)
Plot: (totalPOCost, totalProductRevenue) per supplier
```

Three-table join: purchase_orders + sale_items + products, aggregated by supplier.

## Data Insight

| Supplier | PO Cost | Product Revenue |
|----------|---------|-----------------|
| Pacific Lumber Co | $49,375 | $80,093 |
| Midwest Fastener Supply | $35,202 | $70,370 |
| National Tool Distributors | $77,892 | $137,378 |
| ProPlumb Supply | $50,191 | $100,060 |
| ElectroPro Wholesale | $69,088 | $128,326 |
| ColorMaster Paints | $59,830 | $122,628 |
| GreenGrow Distributors | $93,048 | $180,956 |
| HomeStyle Imports | $35,860 | $72,258 |
| SafeGuard Products | $45,554 | $92,923 |
| BuildRight Materials | $17,826 | $30,602 |
| Apex Tool Group | $55,375 | $106,577 |
| SeasonAll Supply | $52,270 | $94,005 |

Points above the diagonal (revenue > cost) indicate profitable supplier relationships.
The ratio reveals which suppliers deliver the best return on procurement investment.

---

## What Was Built

800x600 viewport, 12 green scatter points. X = total PO cost, Y = total product revenue.

Total: 6 unique IDs.
