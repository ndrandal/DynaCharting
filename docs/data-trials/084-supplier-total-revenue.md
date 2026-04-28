# Trial 084: Supplier Total Revenue

**Date:** 2026-03-22
**Goal:** Three-table join: sale_items -> products -> suppliers. Sum revenue per supplier.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON productId
           JOIN suppliers ON products.supplierId = suppliers.id
GROUP BY suppliers.id
SUM(sale_items.lineTotal) AS revenue
ORDER BY revenue DESC
```

Three-table chain: sale_items (33,834) x products (150) x suppliers (12).

## Data Insight

| Supplier | Revenue |
|----------|---------|
| GreenGrow Distributors | $180,956 |
| National Tool Distributors | $137,378 |
| ElectroPro Wholesale | $128,326 |
| ColorMaster Paints | $122,628 |
| Apex Tool Group | $106,577 |
| ProPlumb Supply | $100,060 |
| SeasonAll Supply | $94,005 |
| SafeGuard Products | $92,923 |
| Pacific Lumber Co | $80,093 |
| HomeStyle Imports | $72,258 |
| Midwest Fastener Supply | $70,370 |
| BuildRight Materials | $30,602 |

The supplier revenue distribution reveals which vendors' products drive the most sales.
This is critical for supplier negotiation leverage — top suppliers may warrant volume discounts.

---

## What Was Built

1000x500 viewport, 12 green bars for supplier revenue, sorted descending.

Total: 6 unique IDs.
